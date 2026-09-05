// Stream the actual LVGL RGB565 flush strips over USB; no full-frame allocation.
#include "usage_capture.h"
#include "src/display/lv_display_private.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>
static lv_display_flush_cb_t original_flush;
static bool capturing;
static bool write_all(const void *data, size_t size)
{
    const char *bytes = data;
    while (size) {
        int written = usb_serial_jtag_write_bytes(bytes, size > 120 ? 120 : size, pdMS_TO_TICKS(1500));
        if (written <= 0) return false;
        bytes += written; size -= written;
    }
    return true;
}
static void capture_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    bool last = lv_display_flush_is_last(display);
    if (capturing) {
        int w = lv_area_get_width(area), h = lv_area_get_height(area);
        char header[96];
        int n = snprintf(header, sizeof(header), "RECT %d %d %d %d %d\n", (int)area->x1, (int)area->y1, w, h, w * h * 2);
        if (!write_all(header, n) || !write_all(pixels, w * h * 2)) {
            capturing = false;
            esp_log_level_set("*", ESP_LOG_INFO);
        }
    }
    original_flush(display, area, pixels);
    if (capturing && last) {
        write_all("END\n", 4); capturing = false;
        esp_log_level_set("*", ESP_LOG_INFO);
    }
}
void usage_capture_install(lv_display_t *display)
{
    // LVGL 9.5 is pinned by dependencies.lock; it has no public flush getter.
    original_flush = display->flush_cb;
    lv_display_set_flush_cb(display, capture_flush);
}
void usage_capture_request(void)
{
    if (capturing) return;
    esp_log_level_set("*", ESP_LOG_NONE);
    if (write_all("USAGE_SHOT 240 320\n", strlen("USAGE_SHOT 240 320\n"))) {
        capturing = true;
        lv_obj_invalidate(lv_screen_active());
    } else esp_log_level_set("*", ESP_LOG_INFO);
}
