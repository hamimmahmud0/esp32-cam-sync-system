#pragma once
#include <stdbool.h>

bool presets_list_json(char *out, int out_max);
bool presets_save_current(const char *name);
bool presets_load_and_apply(const char *name);
