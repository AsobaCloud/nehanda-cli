#ifndef DEC_KB_PATHS_H
#define DEC_KB_PATHS_H 1

const char *kb_default_config_dir(void);
const char *kb_default_socket_path(void);
const char *kb_default_bg_socket_path(void);

/* Backward-compatible names used by existing callers of kb_client.h. */
const char *kb_client_default_socket_path(void);
const char *kb_client_default_bg_socket_path(void);

#endif /* DEC_KB_PATHS_H */
