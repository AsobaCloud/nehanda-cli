#include "hardware_probe.h"
#include "util.h"
#include "db1.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hardware_probe_result_init(hardware_probe_result_t *out)
{
   if (!out)
      return;
   memset(out, 0, sizeof(*out));
   snprintf(out->vendor, sizeof(out->vendor), "unknown");
   snprintf(out->memory_kind, sizeof(out->memory_kind), "unknown");
}

int hardware_probe_parse_nvidia_csv(const char *csv, hardware_probe_result_t *out)
{
   if (!csv || !out)
      return -1;
   hardware_probe_result_init(out);

   char buf[4096];
   snprintf(buf, sizeof(buf), "%s", csv);
   char *saveptr = NULL;
   char *line = strtok_r(buf, "\r\n", &saveptr);
   while (line)
   {
      char *comma = strrchr(line, ',');
      if (comma)
      {
         *comma = '\0';
         char *name = util_trim(line);
         char *mem = util_trim(comma + 1);
         char *end = NULL;
         long mb = strtol(mem, &end, 10);
         if (mb > 0 && mb <= 1024L * 1024L && mb > out->vram_mb)
         {
            out->detected = 1;
            out->vram_mb = (int)mb;
            snprintf(out->name, sizeof(out->name), "%s", name && name[0] ? name : "NVIDIA GPU");
            snprintf(out->vendor, sizeof(out->vendor), "nvidia");
            snprintf(out->memory_kind, sizeof(out->memory_kind), "discrete_vram");
         }
      }
      line = strtok_r(NULL, "\r\n", &saveptr);
   }
   return out->detected ? 0 : -1;
}

int hardware_probe_parse_amd_vram_bytes(const char *bytes_text, int *mb_out)
{
   if (mb_out)
      *mb_out = 0;
   if (!bytes_text || !mb_out)
      return -1;
   char *end = NULL;
   long long bytes = strtoll(bytes_text, &end, 10);
   if (bytes <= 0)
      return -1;
   *mb_out = (int)(bytes / (1024LL * 1024LL));
   return *mb_out > 0 ? 0 : -1;
}

static int hp_detect_nvidia(hardware_probe_result_t *out)
{
   FILE *fp = popen(
       "nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits 2>/dev/null", "r");
   if (!fp)
      return -1;

   char csv[4096] = {0};
   size_t pos = 0;
   while (pos + 1 < sizeof(csv) && fgets(csv + pos, (int)(sizeof(csv) - pos), fp))
      pos = strlen(csv);
   int rc = pclose(fp);
   (void)rc;
   return hardware_probe_parse_nvidia_csv(csv, out);
}

static int hp_read_file(const char *path, char *buf, size_t len)
{
   if (!buf || len == 0)
      return -1;
   buf[0] = '\0';
   FILE *f = fopen(path, "r");
   if (!f)
      return -1;
   size_t n = fread(buf, 1, len - 1, f);
   fclose(f);
   buf[n] = '\0';
   return n > 0 ? 0 : -1;
}

static int hp_detect_amd_sysfs(hardware_probe_result_t *out)
{
   DIR *dir = opendir("/sys/class/drm");
   if (!dir)
      return -1;

   struct dirent *de = NULL;
   int best_mb = 0;
   char best_name[128] = "AMD GPU";
   while ((de = readdir(dir)) != NULL)
   {
      if (strncmp(de->d_name, "card", 4) != 0 || strchr(de->d_name, '-'))
         continue;

      char path[512];
      char vendor[64];
      snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", de->d_name);
      if (hp_read_file(path, vendor, sizeof(vendor)) != 0)
         continue;
      char *v = util_trim(vendor);
      if (strcmp(v, "0x1002") != 0)
         continue;

      char bytes[64];
      snprintf(path, sizeof(path), "/sys/class/drm/%s/device/mem_info_vram_total", de->d_name);
      if (hp_read_file(path, bytes, sizeof(bytes)) != 0)
         continue;
      int mb = 0;
      if (hardware_probe_parse_amd_vram_bytes(bytes, &mb) != 0)
         continue;
      if (mb > best_mb)
      {
         best_mb = mb;
         char product[128];
         snprintf(path, sizeof(path), "/sys/class/drm/%s/device/product_name", de->d_name);
         if (hp_read_file(path, product, sizeof(product)) == 0)
            snprintf(best_name, sizeof(best_name), "%s", util_trim(product));
      }
   }
   closedir(dir);

   if (best_mb <= 0)
      return -1;
   out->detected = 1;
   out->vram_mb = best_mb;
   snprintf(out->name, sizeof(out->name), "%s", best_name);
   snprintf(out->vendor, sizeof(out->vendor), "amd");
   snprintf(out->memory_kind, sizeof(out->memory_kind), "discrete_vram");
   return 0;
}

int hardware_probe_detect(hardware_probe_result_t *out)
{
   if (!out)
      return -1;
   hardware_probe_result_init(out);
   if (hp_detect_nvidia(out) == 0)
      return 0;
   hardware_probe_result_init(out);
   if (hp_detect_amd_sysfs(out) == 0)
      return 0;
   snprintf(out->error, sizeof(out->error), "no NVIDIA or AMD discrete GPU detected");
   return 0;
}

int hardware_probe_cache_result(const hardware_probe_result_t *result)
{
   if (!result)
      return -1;
   char vram[32];
   snprintf(vram, sizeof(vram), "%d", result->detected ? result->vram_mb : 0);
   int rc = 0;
   rc |= db1_env_capability_set("gpu_vram_mb", vram);
   rc |= db1_env_capability_set("gpu_name", result->detected ? result->name : "unknown");
   rc |= db1_env_capability_set("gpu_vendor", result->detected ? result->vendor : "unknown");
   rc |= db1_env_capability_set("memory_kind", result->detected ? result->memory_kind : "unknown");
   return rc == 0 ? 0 : -1;
}

int hardware_probe_cached_or_detect(hardware_probe_result_t *out)
{
   if (!out)
      return -1;
   hardware_probe_result_init(out);

   char value[512];
   if (db1_env_capability_get("gpu_vram_mb", value, sizeof(value), NULL, 0) == 1)
   {
      int mb = atoi(value);
      if (mb <= 0)
      {
         snprintf(out->error, sizeof(out->error), "no NVIDIA or AMD discrete GPU detected");
         return 0;
      }
      if (mb > 0)
      {
         out->detected = 1;
         out->vram_mb = mb;
         if (db1_env_capability_get("gpu_name", value, sizeof(value), NULL, 0) == 1 && value[0])
            snprintf(out->name, sizeof(out->name), "%s", value);
         if (db1_env_capability_get("gpu_vendor", value, sizeof(value), NULL, 0) == 1 && value[0])
            snprintf(out->vendor, sizeof(out->vendor), "%s", value);
         if (db1_env_capability_get("memory_kind", value, sizeof(value), NULL, 0) == 1 && value[0])
            snprintf(out->memory_kind, sizeof(out->memory_kind), "%s", value);
         return 0;
      }
   }

   int rc = hardware_probe_detect(out);
   if (rc == 0)
      (void)hardware_probe_cache_result(out);
   return rc;
}

int hardware_probe_context_window_from_vram(int vram_mb, int model_context_window)
{
   if (vram_mb <= 0)
      return 0;
   int ctx = 4096;
   if (vram_mb >= 24576)
      ctx = 65536;
   else if (vram_mb >= 16384)
      ctx = 32768;
   else if (vram_mb >= 12288)
      ctx = 16384;
   else if (vram_mb >= 8192)
      ctx = 8192;
   if (model_context_window > 0 && ctx > model_context_window)
      ctx = model_context_window;
   return ctx;
}

double hardware_probe_quant_bpw(const char *model)
{
   if (!model || !model[0])
      return 0.0;
   if (str_contains_ci(model, "Q8_0"))
      return 8.0;
   if (str_contains_ci(model, "Q6_K"))
      return 6.57;
   if (str_contains_ci(model, "Q5_K") || str_contains_ci(model, "Q5_"))
      return 5.5;
   if (str_contains_ci(model, "Q4_K") || str_contains_ci(model, "Q4_"))
      return 4.83;
   if (str_contains_ci(model, "Q3_K") || str_contains_ci(model, "Q3_"))
      return 3.9;
   if (str_contains_ci(model, "Q2_K") || str_contains_ci(model, "Q2_"))
      return 3.0;
   if (str_contains_ci(model, "fp16") || str_contains_ci(model, "f16"))
      return 16.0;
   return 0.0;
}

double hardware_probe_params_billion(const char *model)
{
   if (!model)
      return 0.0;
   double best = 0.0;
   const char *p = model;
   while (*p)
   {
      if (isdigit((unsigned char)*p))
      {
         char *end = NULL;
         double val = strtod(p, &end);
         if (end && (*end == 'b' || *end == 'B') && val > best)
            best = val;
         p = end && end > p ? end : p + 1;
      }
      else
         p++;
   }
   return best;
}

int hardware_probe_estimate_model_vram_mb(const char *model, int context_window)
{
   double params_b = hardware_probe_params_billion(model);
   double bpw = hardware_probe_quant_bpw(model);
   if (params_b <= 0.0 || bpw <= 0.0)
      return 0;
   double weights_mb = (params_b * 1000000000.0 * bpw / 8.0) / (1024.0 * 1024.0);
   double kv_mb = context_window > 0 ? ((double)context_window / 1024.0) * 16.0 : 0.0;
   return (int)(weights_mb + kv_mb + 512.0);
}

int hardware_probe_should_warn_fit(const hardware_probe_result_t *result, const char *model,
                                   int context_window, int *estimate_mb_out)
{
   if (estimate_mb_out)
      *estimate_mb_out = 0;
   if (!result || !result->detected || result->vram_mb <= 0)
      return 0;
   int estimate = hardware_probe_estimate_model_vram_mb(model, context_window);
   if (estimate_mb_out)
      *estimate_mb_out = estimate;
   if (estimate <= 0)
      return 0;
   return estimate > (result->vram_mb * 9) / 10;
}
