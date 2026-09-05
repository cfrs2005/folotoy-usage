// Open UsageHub display. Configuration arrives over USB, never in firmware.
#include "usage_model.h"
#include "usage_capture.h"
#include "usage_ui.h"
#include "lvgl.h"
#include <assert.h>
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "cJSON.h"
#include "driver/usb_serial_jtag.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(usage_font);
#define PAPER 0xF6F3EB
#define INK 0x20252B
#define RED 0xBD433C
#define BLUE 0x256AA2
#define CONNECTED BIT0
#define REFRESH BIT1
static EventGroupHandle_t events;
static QueueHandle_t keys;
static usage_snapshot_t snapshot;
static bool have_snapshot;
static int page;
static bool remaining;
static unsigned view_revision;
typedef struct { bsp_btn_t button; bool held; } usage_key_t;
static int64_t fetched_us;
static char status[80] = "请用 USB 配置";
static char ssid[33], password[65], origin[192], token[257];
static char response[32769];
static size_t response_size;
static bool response_overflow;

static void draw(lv_timer_t *timer)
{
    (void)timer;
    static time_t shown_second = -1;
    static unsigned shown_revision;
    static int battery = -1;
    static int64_t battery_read_us;
    bool changed = false;
    usage_key_t key;
    if (xQueueReceive(keys, &key, 0) == pdTRUE) {
        changed = true;
        if (key.button == BSP_BTN_OK && (key.held || page != 0)) xEventGroupSetBits(events, REFRESH);
        else if (key.button == BSP_BTN_OK) remaining = !remaining;
        else page = (page + 1) % 2;
    }
    time_t now = time(NULL);
    if (!changed && shown_second == now && shown_revision == view_revision) return;
    int64_t uptime = esp_timer_get_time();
    if (!battery_read_us || uptime - battery_read_us >= 10000000) {
        battery = bsp_battery_soc(); battery_read_us = uptime;
    }
    usage_ui_update(&snapshot, have_snapshot,
                    have_snapshot && uptime - fetched_us > 180000000,
                    status, page, battery, now, remaining);
    shown_second = now; shown_revision = view_revision;
}
static void set_status(const char *value)
{
    if (bsp_lvgl_lock(1000)) {
        snprintf(status, sizeof(status), "%s", value);
        view_revision++;
        bsp_lvgl_unlock();
    }
}
static void on_key(bsp_btn_t key, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (event == BSP_BTN_CLICK || (key == BSP_BTN_OK && event == BSP_BTN_LONG)) {
        usage_key_t input = {.button=key, .held=event==BSP_BTN_LONG};
        xQueueSend(keys, &input, 0);
    }
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == IP_EVENT) xEventGroupSetBits(events, CONNECTED);
    else if (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(events, CONNECTED);
        esp_wifi_connect();
    }
}
static bool load_config(void)
{
    nvs_handle_t handle;
    if (nvs_open("usage", NVS_READONLY, &handle) != ESP_OK) return false;
    size_t n = sizeof(response);
    esp_err_t err = nvs_get_str(handle, "config", response, &n);
    nvs_close(handle);
    if (err != ESP_OK) return false;
    cJSON *json = cJSON_Parse(response);
    const char *names[] = {"ssid", "password", "origin", "token"};
    char *out[] = {ssid, password, origin, token};
    size_t caps[] = {sizeof(ssid), sizeof(password), sizeof(origin), sizeof(token)};
    bool ok = json != NULL;
    for (int i = 0; i < 4; i++) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(json, names[i]);
        if (!cJSON_IsString(v) || strlen(v->valuestring) >= caps[i]) { ok = false; break; }
        snprintf(out[i], caps[i], "%s", v->valuestring);
    }
    cJSON_Delete(json); memset(response, 0, sizeof(response));
    return ok && ssid[0] && strncmp(origin, "https://", 8) == 0 &&
           (!token[0] || strncmp(token, "uh_display_", 11) == 0);
}
static bool valid_config(const cJSON *root)
{
    const char *names[] = {"ssid", "password", "origin", "token"};
    const size_t limits[] = {32, 64, 180, 256};
    for (int i = 0; i < 4; i++) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, names[i]);
        if (!cJSON_IsString(v) || strlen(v->valuestring) > limits[i]) return false;
        if (i != 1 && i != 3 && !v->valuestring[0]) return false;
        for (const unsigned char *p = (const unsigned char *)v->valuestring; *p; p++)
            if (*p < 32 || *p == 127) return false;
    }
    const char *url = cJSON_GetObjectItemCaseSensitive(root, "origin")->valuestring;
    const char *key = cJSON_GetObjectItemCaseSensitive(root, "token")->valuestring;
    return strncmp(url, "https://", 8) == 0 && url[8] && !strpbrk(url + 8, "/@?# ") &&
           (!key[0] || (strncmp(key, "uh_display_", 11) == 0 && strlen(key) > 11));
}
static void usb_config(void *arg)
{
    (void)arg;
    static char line[8192]; size_t length = 0; bool overflow = false;
    while (true) {
        char byte;
        if (usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(100)) <= 0) continue;
        if (byte != '\n') {
            if (length + 1 < sizeof(line)) line[length++] = byte; else overflow = true;
            continue;
        }
        line[length] = 0;
        cJSON *json = overflow ? NULL : cJSON_Parse(line);
        if (json && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "diagnostics"))) {
            if (bsp_lvgl_lock(1000)) {
                lv_mem_monitor_t memory; lv_mem_monitor(&memory);
                char report[120];
                int n = snprintf(report,sizeof(report),"USAGE_STATUS {\"page\":%d,\"remaining\":%s,\"data\":%s,\"uiFree\":%u,\"heapFree\":%u}\n",page,remaining?"true":"false",have_snapshot?"true":"false",(unsigned)memory.free_size,(unsigned)esp_get_free_heap_size());
                bsp_lvgl_unlock();
                if (n > 0 && n < sizeof(report)) usb_serial_jtag_write_bytes(report,n,pdMS_TO_TICKS(200));
            }
            cJSON_Delete(json); memset(line,0,sizeof(line)); length=0; overflow=false; continue;
        }
        if (json && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "timezone"))) {
            const char *zone = cJSON_GetObjectItemCaseSensitive(json, "timezone")->valuestring;
            bool ok = strlen(zone) > 0 && strlen(zone) < 80;
            nvs_handle_t handle;
            if (ok) {
                ok = nvs_open("usage", NVS_READWRITE, &handle) == ESP_OK;
                if (ok) { ok = nvs_set_str(handle, "timezone", zone) == ESP_OK && nvs_commit(handle) == ESP_OK; nvs_close(handle); }
            }
            if (ok && bsp_lvgl_lock(1000)) { setenv("TZ", zone, 1); tzset(); bsp_lvgl_unlock(); }
            const char *reply = ok ? "USAGE_TIMEZONE_OK\n" : "USAGE_TIMEZONE_ERROR\n";
            usb_serial_jtag_write_bytes(reply, strlen(reply), pdMS_TO_TICKS(200));
            cJSON_Delete(json); memset(line, 0, sizeof(line)); length = 0; overflow = false; continue;
        }
        if (json && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "key"))) {
            const char *key = cJSON_GetObjectItemCaseSensitive(json, "key")->valuestring;
            usage_key_t input = {.button=strcmp(key,"up")==0?BSP_BTN_UP:strcmp(key,"down")==0?BSP_BTN_DOWN:BSP_BTN_OK, .held=strcmp(key,"hold")==0};
            if (strcmp(key,"up")==0 || strcmp(key,"down")==0 || strcmp(key,"ok")==0 || strcmp(key,"hold")==0) xQueueSend(keys,&input,0);
            cJSON_Delete(json); memset(line, 0, sizeof(line)); length = 0; overflow = false; continue;
        }
        if (json && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "avatar"))) {
            const char *hex = cJSON_GetObjectItemCaseSensitive(json, "avatar")->valuestring;
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "displayName");
            uint8_t pixels[3200]; size_t bytes = strlen(hex)/2; bool ok = strlen(hex)%2==0 && (bytes==1568 || bytes==3200);
            for (size_t i = 0; ok && i < bytes; i++) {
                char part[3] = {hex[i*2], hex[i*2+1], 0}; char *end;
                long value = strtol(part, &end, 16);
                if (*end || end == part) ok = false; else pixels[i] = value;
            }
            nvs_handle_t handle;
            if (ok) {
                ok = nvs_open("usage", NVS_READWRITE, &handle) == ESP_OK;
                if (ok) {
                    ok = nvs_set_blob(handle, "avatar", pixels, bytes) == ESP_OK;
                    if (ok && cJSON_IsString(name) && strlen(name->valuestring) <= 48)
                        ok = nvs_set_str(handle, "name", name->valuestring) == ESP_OK;
                    if (ok) ok = nvs_commit(handle) == ESP_OK;
                    nvs_close(handle);
                }
                if (ok && bsp_lvgl_lock(1000)) {
                    usage_ui_set_profile(cJSON_IsString(name) ? name->valuestring : NULL, pixels, bytes);
                    bsp_lvgl_unlock();
                }
            }
            const char *reply = ok ? "USAGE_PROFILE_OK\n" : "USAGE_PROFILE_ERROR\n";
            usb_serial_jtag_write_bytes(reply, strlen(reply), pdMS_TO_TICKS(200));
            cJSON_Delete(json); memset(line, 0, sizeof(line)); length = 0; overflow = false;
            continue;
        }
        if (json && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "screenshot"))) {
            if (bsp_lvgl_lock(1000)) { usage_capture_request(); bsp_lvgl_unlock(); }
            cJSON_Delete(json); memset(line, 0, sizeof(line)); length = 0; overflow = false;
            continue;
        }
        // A Wi-Fi-only update preserves any previously paired display token.
        if (json && cJSON_IsObject(json)) {
            char previous[1024]; size_t size = sizeof(previous);
            nvs_handle_t saved;
            cJSON *old = NULL;
            if (nvs_open("usage", NVS_READONLY, &saved) == ESP_OK) {
                if (nvs_get_str(saved, "config", previous, &size) == ESP_OK)
                    old = cJSON_Parse(previous);
                nvs_close(saved);
            }
            const char *optional[] = {"origin", "token"};
            const char *defaults[] = {"https://u.80aj.com", ""};
            for (int i = 0; i < 2; i++) {
                if (!cJSON_GetObjectItemCaseSensitive(json, optional[i])) {
                    const cJSON *value = cJSON_GetObjectItemCaseSensitive(old, optional[i]);
                    cJSON_AddStringToObject(json, optional[i],
                                           cJSON_IsString(value) ? value->valuestring : defaults[i]);
                }
            }
            cJSON_Delete(old); memset(previous, 0, sizeof(previous));
        }
        bool ok = json && valid_config(json);
        char *serialized = ok ? cJSON_PrintUnformatted(json) : NULL;
        ok = ok && serialized != NULL;
        nvs_handle_t handle;
        if (ok) {
            ok = nvs_open("usage", NVS_READWRITE, &handle) == ESP_OK;
            if (ok) {
                ok = nvs_set_str(handle, "config", serialized) == ESP_OK && nvs_commit(handle) == ESP_OK;
                nvs_close(handle);
            }
        }
        if (serialized) { memset(serialized, 0, strlen(serialized)); cJSON_free(serialized); }
        cJSON_Delete(json); memset(line, 0, sizeof(line)); length = 0; overflow = false;
        const char *reply = ok ? "USAGE_CONFIG_OK\n" : "USAGE_CONFIG_ERROR\n";
        usb_serial_jtag_write_bytes(reply, strlen(reply), pdMS_TO_TICKS(200));
        if (ok) { vTaskDelay(pdMS_TO_TICKS(500)); esp_restart(); }
    }
}
static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        if ((size_t)event->data_len > sizeof(response) - 1 - response_size) {
            response_overflow = true; return ESP_FAIL;
        }
        memcpy(response + response_size, event->data, event->data_len);
        response_size += event->data_len; response[response_size] = 0;
    }
    return ESP_OK;
}
static void network(void *arg)
{
    (void)arg;
    if (!load_config()) { set_status("请用 USB 配置"); vTaskDelete(NULL); return; }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, ssid, strlen(ssid));
    memcpy(cfg.sta.password, password, strlen(password));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org"); esp_sntp_init();
    char url[224], auth[272];
    snprintf(url, sizeof(url), "%s/v1/dashboard", origin);
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    while (true) {
        set_status("连接中");
        EventBits_t bits = xEventGroupWaitBits(events, CONNECTED, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));
        if (!(bits & CONNECTED)) set_status("离线 保留上次数据");
        else if (!token[0]) set_status("连接正常 请配置授权");
        else if (time(NULL) < 1700000000) set_status("等待校时");
        else {
            set_status("更新中"); response_size = 0; response_overflow = false;
            esp_http_client_config_t http = {
                .url = url, .timeout_ms = 15000, .event_handler = http_event,
                .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
                .buffer_size = 1024, .buffer_size_tx = 1024,
            };
            esp_http_client_handle_t client = esp_http_client_init(&http);
            esp_err_t error = ESP_FAIL; int code = 0;
            if (client) {
                esp_http_client_set_header(client, "Authorization", auth);
                error = esp_http_client_perform(client);
                code = esp_http_client_get_status_code(client);
                esp_http_client_cleanup(client);
            }
            usage_snapshot_t next;
            if (error == ESP_OK && code == 200 && !response_overflow && usage_parse(response, response_size, &next)) {
                if (bsp_lvgl_lock(1000)) {
                    snapshot = next; have_snapshot = true; fetched_us = esp_timer_get_time(); view_revision++;
                    snprintf(status, sizeof(status), "已更新 %.5s UTC", next.server_time + 11);
                    bsp_lvgl_unlock();
                    ESP_LOGI("usage", "Dashboard snapshot received");
                }
            } else {
                ESP_LOGW("usage", "Dashboard fetch failed: transport=%d http=%d overflow=%d", error, code, response_overflow);
                set_status(code == 401 || code == 403 ? "授权失效 请重新配置" : "更新失败 保留上次数据");
            }
        }
        xEventGroupWaitBits(events, REFRESH, pdTRUE, pdFALSE, pdMS_TO_TICKS(60000));
    }
}
void app_main(void)
{
    // Never erase NVS automatically: a storage error must not remove user data.
    ESP_ERROR_CHECK(nvs_flash_init());
    events = xEventGroupCreate(); keys = xQueueCreate(6, sizeof(usage_key_t));
    assert(events && keys);
    bsp_i2c_init();
    ESP_ERROR_CHECK(bsp_display_init());
    assert(bsp_lvgl_init()); bsp_display_backlight(60); bsp_battery_init();
    if (bsp_lvgl_lock(1000)) {
        usage_ui_create();
        nvs_handle_t profile;
        if (nvs_open("usage", NVS_READONLY, &profile) == ESP_OK) {
            char zone[80]; size_t zone_size = sizeof(zone);
            if (nvs_get_str(profile, "timezone", zone, &zone_size) == ESP_OK) { setenv("TZ", zone, 1); tzset(); }
            static uint8_t pixels[3200]; size_t count = sizeof(pixels);
            char name[49] = {0}; size_t name_size = sizeof(name);
            nvs_get_str(profile, "name", name, &name_size);
            if (nvs_get_blob(profile, "avatar", pixels, &count) == ESP_OK)
                usage_ui_set_profile(name, pixels, count);
            nvs_close(profile);
        }
        lv_timer_create(draw, 50, NULL);
        usage_capture_install(lv_display_get_default());
        bsp_lvgl_unlock();
    }
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 256, .rx_buffer_size = 2048};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    assert(xTaskCreate(usb_config, "usage-config", 8192, NULL, 4, NULL) == pdPASS);
    assert(xTaskCreate(network, "usage-fetch", 8192, NULL, 3, NULL) == pdPASS);
}
