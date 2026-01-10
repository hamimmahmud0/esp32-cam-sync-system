#pragma once
#include <stdbool.h>

bool slave_http_post_json(const char *path, const char *json_body);
bool slave_http_get(const char *path, char *out, int out_max);
