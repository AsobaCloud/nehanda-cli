# health_response_t

## Properties
Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **char \*** | ok when healthy; a non-ok string when degraded | 
**db2_ok** | **int** | DB2 (Postgres) reachable | [optional] 
**db2_kb_tables_ok** | **int** | required kb tables present | [optional] 
**pgvec_ok** | **int** | pgvector extension available | [optional] 
**pgvec_collection_ok** | **int** |  | [optional] 
**embed_ok** | **int** |  | [optional] 
**embed_command** | **char \*** |  | [optional] 
**chunk_count** | **int** |  | [optional] 
**embedding_count** | **int** |  | [optional] 
**warnings** | **list_t \*** |  | [optional] 

[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


