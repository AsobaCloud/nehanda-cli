/* osv_check.h: OSV malware gate helpers for package-manager MCP launches. */
#ifndef DEC_OSV_CHECK_H
#define DEC_OSV_CHECK_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define OSV_ECOSYSTEM_LEN 32
#define OSV_NAME_LEN      192
#define OSV_VERSION_LEN   64
#define OSV_ADVISORY_LEN  256

   typedef struct
   {
      char ecosystem[OSV_ECOSYSTEM_LEN];
      char name[OSV_NAME_LEN];
      char version[OSV_VERSION_LEN];
   } osv_target_t;

   typedef enum
   {
      OSV_VERDICT_UNKNOWN = 0,
      OSV_VERDICT_CLEAN = 1,
      OSV_VERDICT_MALWARE = 2
   } osv_verdict_t;

   typedef struct
   {
      osv_verdict_t verdict;
      char advisory_ids[OSV_ADVISORY_LEN];
   } osv_result_t;

   int osv_infer_target_from_argv(int argc, const char *const argv[], osv_target_t *out);
   int osv_response_has_malware(const char *json, char *advisory_ids, size_t advisory_cap);
   osv_result_t osv_query_target(const char *endpoint, const osv_target_t *target, int timeout_ms);
   osv_result_t osv_check_cached(const char *endpoint, const osv_target_t *target, int ttl_hours,
                                 int offline, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* DEC_OSV_CHECK_H */
