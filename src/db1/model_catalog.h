/* db1/model_catalog.h: provider model catalog cache. */
#ifndef DEC_DB1_MODEL_CATALOG_H
#define DEC_DB1_MODEL_CATALOG_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   int db1_model_catalog_is_fresh(const char *provider, int ttl_seconds);
   int db1_model_catalog_get(const char *provider, char ***models_out, int *n_out);
   int db1_model_catalog_replace(const char *provider, char **models, int n);
   void db1_model_catalog_free(char **models, int n);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB1_MODEL_CATALOG_H */
