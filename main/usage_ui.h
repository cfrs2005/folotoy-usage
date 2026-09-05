#pragma once
#include "usage_model.h"
#include "lvgl.h"
#include <time.h>
void usage_ui_create(void);
void usage_ui_update(const usage_snapshot_t *data, bool available, bool old,
                     const char *status, int page, int battery, time_t now, bool remaining);
void usage_ui_set_profile(const char *name, const uint8_t *rgb565, size_t bytes);
