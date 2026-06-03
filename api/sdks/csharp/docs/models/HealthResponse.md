# AimeeKb.Model.HealthResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | **string** | ok when healthy; a non-ok string when degraded | 
**Db2Ok** | **bool** | DB2 (Postgres) reachable | [optional] 
**Db2KbTablesOk** | **bool** | required kb tables present | [optional] 
**PgvecOk** | **bool** | pgvector extension available | [optional] 
**PgvecCollectionOk** | **bool** |  | [optional] 
**EmbedOk** | **bool** |  | [optional] 
**EmbedCommand** | **string** |  | [optional] 
**ChunkCount** | **int** |  | [optional] 
**EmbeddingCount** | **int** |  | [optional] 
**Warnings** | **List&lt;string&gt;** |  | [optional] 

[[Back to Model list]](../../README.md#documentation-for-models) [[Back to API list]](../../README.md#documentation-for-api-endpoints) [[Back to README]](../../README.md)

