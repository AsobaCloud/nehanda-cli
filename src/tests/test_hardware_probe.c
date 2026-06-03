#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db1.h"
#include "hardware_probe.h"

static void test_nvidia_csv_parse(void)
{
   hardware_probe_result_t hw;
   assert(hardware_probe_parse_nvidia_csv("RTX 4090, 24564\nRTX 3060, 12288\n", &hw) == 0);
   assert(hw.detected == 1);
   assert(hw.vram_mb == 24564);
   assert(strcmp(hw.vendor, "nvidia") == 0);
   assert(strcmp(hw.memory_kind, "discrete_vram") == 0);
   assert(strcmp(hw.name, "RTX 4090") == 0);
}

static void test_amd_vram_parse(void)
{
   int mb = 0;
   assert(hardware_probe_parse_amd_vram_bytes("17179869184\n", &mb) == 0);
   assert(mb == 16384);
   assert(hardware_probe_parse_amd_vram_bytes("not-a-number", &mb) != 0);
}

static void test_vram_context_tiers(void)
{
   assert(hardware_probe_context_window_from_vram(0, 0) == 0);
   assert(hardware_probe_context_window_from_vram(6144, 0) == 4096);
   assert(hardware_probe_context_window_from_vram(8192, 0) == 8192);
   assert(hardware_probe_context_window_from_vram(12288, 0) == 16384);
   assert(hardware_probe_context_window_from_vram(24576, 0) == 65536);
   assert(hardware_probe_context_window_from_vram(24576, 32768) == 32768);
}

static void test_model_estimate(void)
{
   assert(hardware_probe_quant_bpw("Qwen2.5-7B-Q4_K_M.gguf") > 4.0);
   assert(hardware_probe_params_billion("gemma-4-26B-A4B-it-UD-Q5_K_XL.gguf") == 26.0);
   int estimate = hardware_probe_estimate_model_vram_mb("Qwen_Qwen3.6-35B-A3B-Q5_K_M.gguf", 32768);
   assert(estimate > 20000);

   hardware_probe_result_t hw;
   hardware_probe_result_init(&hw);
   hw.detected = 1;
   hw.vram_mb = 8192;
   snprintf(hw.vendor, sizeof(hw.vendor), "nvidia");
   int estimate_out = 0;
   assert(hardware_probe_should_warn_fit(&hw, "Qwen_Qwen3.6-35B-A3B-Q5_K_M.gguf", 32768,
                                         &estimate_out) == 1);
   assert(estimate_out == estimate);
}

static void test_env_capability_get(void)
{
   assert(db1_init(":memory:") == 0);
   assert(db1_env_capability_get("gpu_vram_mb", NULL, 0, NULL, 0) == 0);
   assert(db1_env_capability_set("gpu_vram_mb", "24564") == 0);
   char value[64];
   assert(db1_env_capability_get("gpu_vram_mb", value, sizeof(value), NULL, 0) == 1);
   assert(strcmp(value, "24564") == 0);
   db1_shutdown();
}

static void test_cached_no_gpu_is_terminal(void)
{
   assert(db1_init(":memory:") == 0);
   hardware_probe_result_t hw;
   hardware_probe_result_init(&hw);
   hw.detected = 0;
   assert(hardware_probe_cache_result(&hw) == 0);

   hardware_probe_result_t cached;
   assert(hardware_probe_cached_or_detect(&cached) == 0);
   assert(cached.detected == 0);
   assert(cached.vram_mb == 0);
   assert(strstr(cached.error, "no NVIDIA or AMD") != NULL);
   db1_shutdown();
}

int main(void)
{
   test_nvidia_csv_parse();
   test_amd_vram_parse();
   test_vram_context_tiers();
   test_model_estimate();
   test_env_capability_get();
   test_cached_no_gpu_is_terminal();
   printf("hardware_probe: all tests passed\n");
   return 0;
}
