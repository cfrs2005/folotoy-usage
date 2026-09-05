#include "usage_model.h"
#include "cJSON.h"
#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const cJSON *get(const cJSON *obj, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}
static bool equal(const cJSON *obj, const char *key, const char *value)
{
    const cJSON *v = get(obj, key);
    return cJSON_IsString(v) && strcmp(v->valuestring, value) == 0;
}
static double number(const cJSON *obj, const char *key)
{
    const cJSON *v = get(obj, key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= 0 &&
           v->valuedouble <= 9007199254740991.0 ? v->valuedouble : -1;
}
bool usage_parse(const char *json, size_t length, usage_snapshot_t *out)
{
    if (!json || !out || !length || length > 32768) return false;
    /* Bound recursion before entering cJSON on the small ESP32 task stack. */
    int depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = 0; i < length; i++) {
        char c = json[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 16) return false; }
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
    }
    if (depth || quoted) return false;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, length, &end, false);
    if (!root) return false;
    while (end < json + length && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) end++;
    const cJSON *providers = get(root, "providers"), *stamp = get(root, "serverTime");
    if (end != json + length || !cJSON_IsObject(root) || !cJSON_IsArray(providers) ||
        !cJSON_IsString(stamp) || strlen(stamp->valuestring) >= 32) {
        cJSON_Delete(root);
        return false;
    }
    usage_snapshot_t next = {0};
    snprintf(next.server_time, sizeof(next.server_time), "%s", stamp->valuestring);
    for (int i = 0; i < 2; i++) {
        next.providers[i].stale = next.providers[i].tokens_stale = true;
        next.providers[i].today_tokens = next.providers[i].total_tokens = -1;
        next.providers[i].today_cost = next.providers[i].total_cost = -1;
    }
    bool seen[2] = {false};
    const cJSON *p;
    cJSON_ArrayForEach(p, providers) {
        int index = equal(p, "provider", "claude") ? 0 : equal(p, "provider", "codex") ? 1 : -1;
        if (index < 0) continue;
        if (seen[index]) { cJSON_Delete(root); return false; }
        seen[index] = true;
        usage_provider_t *dest = &next.providers[index];
        dest->present = equal(p, "status", "ok");
        dest->stale = !cJSON_IsFalse(get(p, "stale"));
        const cJSON *windows = get(p, "windows"), *w;
        if (!cJSON_IsArray(windows) || cJSON_GetArraySize(windows) > USAGE_WINDOWS) {
            cJSON_Delete(root); return false;
        }
        cJSON_ArrayForEach(w, windows) {
            usage_window_t *window = &dest->windows[dest->count++];
            const cJSON *id = get(w, "limitId");
            snprintf(window->id, sizeof(window->id), "%s", cJSON_IsString(id) ? id->valuestring : "?");
            double minutes = number(w, "durationMinutes");
            window->minutes = minutes > 0 && minutes <= 525600 ? (int)minutes : 0;
            window->used = number(w, "usedPercent");
            if (window->used > 10000) window->used = -1;
            double reset = number(w, "resetsAt");
            window->reset = reset >= 0 ? (int64_t)reset : -1;
            if (window->used < 0 || window->reset < 0 || !window->minutes) dest->stale = true;
        }
        if (!dest->count) dest->stale = true;
        const cJSON *t = get(p, "tokenUsage");
        dest->tokens_ok = equal(t, "status", "ok");
        dest->tokens_stale = !cJSON_IsFalse(get(t, "stale"));
        if (dest->tokens_ok) {
            dest->today_tokens = number(t, "todayTokens"); dest->total_tokens = number(t, "totalTokens");
            dest->today_cost = number(t, "todayCostUsd"); dest->total_cost = number(t, "totalCostUsd");
        }
    }
    *out = next;
    cJSON_Delete(root);
    return true;
}
void usage_number(char *out, size_t capacity, double value, bool money)
{
    if (!isfinite(value) || value < 0) snprintf(out, capacity, "--");
    else if (money) snprintf(out, capacity, "$%.2f", value);
    else if (value >= 1e9) snprintf(out, capacity, "%.2fB", value / 1e9);
    else if (value >= 1e6) snprintf(out, capacity, "%.2fM", value / 1e6);
    else if (value >= 1e3) snprintf(out, capacity, "%.1fK", value / 1e3);
    else snprintf(out, capacity, "%.0f", value);
}

const usage_window_t *usage_select_window(const usage_provider_t *provider, bool codex, int minutes)
{
    const usage_window_t *selected = NULL;
    int best = 4;
    for (int i = 0; i < provider->count; i++) {
        const usage_window_t *w = &provider->windows[i];
        if (w->minutes != minutes) continue;
        int rank = 3;
        size_t n = strlen(w->id);
        if (codex && strcmp(w->id, "codex:primary") == 0) rank = 0;
        else if (codex && strncmp(w->id, "codex:", 6) == 0) rank = 1;
        else if (codex && strncmp(w->id, "codex_", 6) == 0 && n >= 8 && strcmp(w->id + n - 8, ":primary") == 0) rank = 2;
        if (rank < best) { best = rank; selected = w; }
    }
    if (selected) return selected;
    const char *fallback = minutes == 300 ? "five_hour" : "seven_day";
    for (int i = 0; i < provider->count; i++)
        if (strcmp(provider->windows[i].id, fallback) == 0) return &provider->windows[i];
    return NULL;
}

void usage_reset_text(char *out, size_t capacity, int64_t reset, int64_t now)
{
    if (reset < 0 || now < 1700000000) { snprintf(out, capacity, "重置时间未知"); return; }
    if (reset <= now) { snprintf(out, capacity, "等待额度刷新"); return; }
    int64_t minutes = (reset - now + 59) / 60;
    if (minutes >= 1440) snprintf(out, capacity, "还剩 %" PRId64 "天%02" PRId64 "时重置", minutes / 1440, minutes / 60 % 24);
    else if (minutes >= 60) snprintf(out, capacity, "还剩 %" PRId64 "时%02" PRId64 "分重置", minutes / 60, minutes % 60);
    else snprintf(out, capacity, "还剩 %" PRId64 "分钟重置", minutes);
}

double usage_display_percent(double used, bool remaining)
{
    if (!isfinite(used) || used < 0) return -1;
    return remaining ? fmax(0, 100 - used) : used;
}
