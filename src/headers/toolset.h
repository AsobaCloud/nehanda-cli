#ifndef DEC_TOOLSET_H
#define DEC_TOOLSET_H 1

#include <stddef.h>

#define TOOLSET_NAME_MAX    64
#define TOOLSET_TOOL_MAX    128
#define TOOLSET_MAX_SETS    64
#define TOOLSET_MAX_INCLUDE 16
#define TOOLSET_MAX_TOOLS   128
#define TOOLSET_ERROR_MAX   256

typedef struct
{
   char name[TOOLSET_NAME_MAX];
   char include[TOOLSET_MAX_INCLUDE][TOOLSET_NAME_MAX];
   int include_count;
   char tools[TOOLSET_MAX_TOOLS][TOOLSET_TOOL_MAX];
   int tool_count;
   int builtin;
} toolset_def_t;

typedef struct
{
   toolset_def_t sets[TOOLSET_MAX_SETS];
   int count;
   char script_allowed_tools[TOOLSET_NAME_MAX];
} toolset_registry_t;

void toolset_registry_init(toolset_registry_t *registry);
int toolset_registry_load_effective(toolset_registry_t *registry, char *err, size_t err_len);
int toolset_registry_load_file(toolset_registry_t *registry, const char *path, char *err,
                               size_t err_len);
const toolset_def_t *toolset_registry_find(const toolset_registry_t *registry, const char *name);
int toolset_registry_validate(const toolset_registry_t *registry, char *err, size_t err_len);
int toolset_resolve(const toolset_registry_t *registry, const char *name,
                    char out[][TOOLSET_TOOL_MAX], int max_tools, char *err, size_t err_len);
int toolset_resolve_effective(const char *name, char out[][TOOLSET_TOOL_MAX], int max_tools,
                              char *err, size_t err_len);
int toolset_tool_known(const char *name);
const char *toolset_for_delegate_role(const char *role);

#endif /* DEC_TOOLSET_H */
