#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define USAGE_WINDOWS 8

typedef struct {
    char id[40];
    int minutes;
    double used;
    int64_t reset;
} usage_window_t;
typedef struct {
    bool present, stale, tokens_ok, tokens_stale;
    usage_window_t windows[USAGE_WINDOWS];
    int count;
    double today_tokens, total_tokens, today_cost, total_cost;
} usage_provider_t;
typedef struct {
    usage_provider_t providers[2];
    char server_time[32];
} usage_snapshot_t;
/* Parse atomically: malformed input never replaces the last good snapshot. */
bool usage_parse(const char *json, size_t length, usage_snapshot_t *out);
void usage_number(char *out, size_t capacity, double value, bool money);

const usage_window_t *usage_select_window(const usage_provider_t *provider, bool codex, int minutes);

void usage_reset_text(char *out, size_t capacity, int64_t reset, int64_t now);

double usage_display_percent(double used, bool remaining);
