#include "usage_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool parse(const char *s, usage_snapshot_t *out) { return usage_parse(s, strlen(s), out); }
int main(void)
{
    usage_snapshot_t result = {0};
    const char *good = "{\"serverTime\":\"2026-09-05T12:00:00Z\",\"providers\":[{\"provider\":\"codex\",\"status\":\"ok\",\"stale\":false,\"windows\":[{\"limitId\":\"codex\",\"durationMinutes\":300,\"usedPercent\":0,\"resetsAt\":1788000000},{\"limitId\":\"review\",\"durationMinutes\":10080,\"usedPercent\":110,\"resetsAt\":1789000000}],\"tokenUsage\":{\"status\":\"ok\",\"stale\":false,\"todayTokens\":0,\"totalTokens\":9000000000,\"todayCostUsd\":null,\"totalCostUsd\":12.5}}]}";
    assert(parse(good, &result));
    assert(!result.providers[0].present);
    assert(result.providers[0].today_tokens == -1);
    assert(result.providers[1].count == 2);
    assert(result.providers[1].windows[0].used == 0);
    assert(result.providers[1].windows[1].used == 110);
    assert(result.providers[1].total_tokens == 9000000000.0);
    assert(result.providers[1].today_cost == -1);
    usage_snapshot_t before = result;
    assert(!parse("{\"providers\":null}", &result));
    assert(memcmp(&before, &result, sizeof(result)) == 0);
    assert(!parse("{}junk", &result));
    assert(!usage_parse(good, strlen(good) - 1, &result));
    assert(!parse("{\"serverTime\":\"x\",\"providers\":[{\"provider\":\"codex\",\"windows\":[]},{\"provider\":\"codex\",\"windows\":[]}]}", &result));
    assert(parse("{\"serverTime\":\"x\",\"providers\":[{\"provider\":\"claude\",\"status\":\"missing\",\"windows\":[],\"tokenUsage\":{\"status\":\"missing\",\"todayTokens\":123}}]}", &result));
    assert(result.providers[0].today_tokens == -1);
    assert(result.providers[0].stale);
    assert(!parse("[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]", &result));
    char text[32];
    usage_number(text, sizeof(text), -1, false); assert(strcmp(text, "--") == 0);
    usage_number(text, sizeof(text), 0, false); assert(strcmp(text, "0") == 0);
    usage_number(text, sizeof(text), 9000000000., false); assert(strcmp(text, "9.00B") == 0);
    usage_number(text, sizeof(text), 12.5, true); assert(strcmp(text, "$12.50") == 0);
    usage_provider_t multiple = {.count = 4, .windows = {
        {.id="base_model_inference:primary", .minutes=10080, .used=0},
        {.id="codex:primary", .minutes=10080, .used=80},
        {.id="codex_bengalfox:primary", .minutes=300, .used=2},
        {.id="codex_bengalfox:secondary", .minutes=10080, .used=0}
    }};
    assert(usage_select_window(&multiple, true, 10080)->used == 80);
    assert(usage_select_window(&multiple, true, 300)->used == 2);
    assert(usage_select_window(&multiple, false, 300)->used == 2);
    assert(usage_select_window(&multiple, true, 60) == NULL);
    usage_reset_text(text, sizeof(text), -1, 1788000000); assert(strcmp(text, "重置时间未知") == 0);
    usage_reset_text(text, sizeof(text), 1788000000, 1788000000); assert(strcmp(text, "等待额度刷新") == 0);
    usage_reset_text(text, sizeof(text), 1788000001, 1788000000); assert(strcmp(text, "还剩 1分钟重置") == 0);
    usage_reset_text(text, sizeof(text), 1788012240, 1788000000); assert(strcmp(text, "还剩 3时24分重置") == 0);
    usage_reset_text(text, sizeof(text), 1788262800, 1788000000); assert(strcmp(text, "还剩 3天01时重置") == 0);
    assert(usage_display_percent(-1, true) == -1);
    assert(usage_display_percent(0, true) == 100);
    assert(usage_display_percent(81, true) == 19);
    assert(usage_display_percent(125, true) == 0);
    assert(usage_display_percent(125, false) == 125);
    puts("Usage model tests: PASS");
}
