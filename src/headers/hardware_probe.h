#ifndef DEC_HARDWARE_PROBE_H
#define DEC_HARDWARE_PROBE_H 1

#include <stddef.h>

typedef struct
{
   int detected;
   int vram_mb;
   char name[128];
   char vendor[32];
   char memory_kind[32];
   char error[256];
} hardware_probe_result_t;

void hardware_probe_result_init(hardware_probe_result_t *out);
int hardware_probe_parse_nvidia_csv(const char *csv, hardware_probe_result_t *out);
int hardware_probe_parse_amd_vram_bytes(const char *bytes_text, int *mb_out);
int hardware_probe_detect(hardware_probe_result_t *out);
int hardware_probe_cache_result(const hardware_probe_result_t *result);
int hardware_probe_cached_or_detect(hardware_probe_result_t *out);
int hardware_probe_context_window_from_vram(int vram_mb, int model_context_window);
double hardware_probe_quant_bpw(const char *model);
double hardware_probe_params_billion(const char *model);
int hardware_probe_estimate_model_vram_mb(const char *model, int context_window);
int hardware_probe_should_warn_fit(const hardware_probe_result_t *result, const char *model,
                                   int context_window, int *estimate_mb_out);

#endif /* DEC_HARDWARE_PROBE_H */
