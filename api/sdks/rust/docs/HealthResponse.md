# HealthResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **String** | ok when healthy; a non-ok string when degraded | 
**db2_ok** | Option<**bool**> | DB2 (Postgres) reachable | [optional]
**db2_kb_tables_ok** | Option<**bool**> | required kb tables present | [optional]
**pgvec_ok** | Option<**bool**> | pgvector extension available | [optional]
**pgvec_collection_ok** | Option<**bool**> |  | [optional]
**embed_ok** | Option<**bool**> |  | [optional]
**embed_command** | Option<**String**> |  | [optional]
**chunk_count** | Option<**i32**> |  | [optional]
**embedding_count** | Option<**i32**> |  | [optional]
**warnings** | Option<**Vec<String>**> |  | [optional]

[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


