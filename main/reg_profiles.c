#include "reg_profiles.h"
#include "app_config.h"
#include "ov2640_ctrl.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG="PRESET";

static void preset_path(char *out, int out_max, const char *name) {
  snprintf(out, out_max, "%s/%s.json", REGPROFILES_DIR, name);
}

bool presets_list_json(char *out, int out_max) {
  DIR *d = opendir(REGPROFILES_DIR);
  if (!d) {
    snprintf(out, out_max, "{\"ok\":false,\"err\":\"no_dir\"}");
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(root, "presets");

  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    const char *n = e->d_name;
    size_t L = strlen(n);
    if (L > 5 && strcmp(n + (L-5), ".json") == 0) {
      char base[128];
      snprintf(base, sizeof(base), "%.*s", (int)(L-5), n);
      cJSON_AddItemToArray(arr, cJSON_CreateString(base));
    }
  }
  closedir(d);

  char *s = cJSON_PrintUnformatted(root);
  snprintf(out, out_max, "%s", s);
  free(s);
  cJSON_Delete(root);
  return true;
}

static bool write_bank_dump(cJSON *arr, ov2640_bank_t bank) {
  for (int a = 0; a < 256; a++) {
    uint8_t v = 0;
    if (!ov2640_read_reg(bank, (uint8_t)a, &v)) return false;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "addr", a);
    cJSON_AddNumberToObject(obj, "val", v);
    cJSON_AddItemToArray(arr, obj);
  }
  return true;
}

bool presets_save_current(const char *name) {
  char path[256];
  preset_path(path, sizeof(path), name);

  cJSON *root = cJSON_CreateObject();
  cJSON *dsp = cJSON_AddArrayToObject(root, "dsp");
  cJSON *sen = cJSON_AddArrayToObject(root, "sensor");

  bool ok = write_bank_dump(dsp, REG_BANK_DSP) && write_bank_dump(sen, REG_BANK_SENSOR);
  if (!ok) { cJSON_Delete(root); return false; }

  char *json = cJSON_Print(root);
  FILE *f = fopen(path, "wb");
  if (!f) { free(json); cJSON_Delete(root); return false; }
  fwrite(json, 1, strlen(json), f);
  fclose(f);

  free(json);
  cJSON_Delete(root);
  ESP_LOGI(TAG, "Saved preset: %s", path);
  return true;
}

static bool apply_bank_array(cJSON *arr, ov2640_bank_t bank) {
  if (!cJSON_IsArray(arr)) return false;
  int n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *obj = cJSON_GetArrayItem(arr, i);
    cJSON *a = cJSON_GetObjectItem(obj, "addr");
    cJSON *v = cJSON_GetObjectItem(obj, "val");
    if (!cJSON_IsNumber(a) || !cJSON_IsNumber(v)) continue;
    if (!ov2640_write_reg(bank, (uint8_t)a->valueint, (uint8_t)v->valueint)) return false;
  }
  return true;
}

bool presets_load_and_apply(const char *name) {
  char path[256];
  preset_path(path, sizeof(path), name);

  FILE *f = fopen(path, "rb");
  if (!f) return false;

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 200000) { fclose(f); return false; }

  char *buf = (char*)malloc(sz + 1);
  if (!buf) { fclose(f); return false; }
  fread(buf, 1, sz, f);
  buf[sz] = 0;
  fclose(f);

  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) return false;

  cJSON *dsp = cJSON_GetObjectItem(root, "dsp");
  cJSON *sen = cJSON_GetObjectItem(root, "sensor");

  bool ok = apply_bank_array(dsp, REG_BANK_DSP) && apply_bank_array(sen, REG_BANK_SENSOR);
  cJSON_Delete(root);
  return ok;
}
