#ifndef DEC_CONFIG_DATABASE_H
#define DEC_CONFIG_DATABASE_H 1

/* Parses the DB2 config block (db2_url, db2_pool_size). DB1 path and
 * other tier-specific config live elsewhere; pgvector lives inside DB2
 * and needs no separate connection URL. */

#include "config.h"
#include "cJSON.h"

void config_parse_database(config_t *cfg, cJSON *root);

#endif
