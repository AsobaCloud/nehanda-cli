

# HealthResponse


## Properties

| Name | Type | Description | Notes |
|------------ | ------------- | ------------- | -------------|
|**status** | **String** | ok when healthy; a non-ok string when degraded |  |
|**db2Ok** | **Boolean** | DB2 (Postgres) reachable |  [optional] |
|**db2KbTablesOk** | **Boolean** | required kb tables present |  [optional] |
|**pgvecOk** | **Boolean** | pgvector extension available |  [optional] |
|**pgvecCollectionOk** | **Boolean** |  |  [optional] |
|**embedOk** | **Boolean** |  |  [optional] |
|**embedCommand** | **String** |  |  [optional] |
|**chunkCount** | **Integer** |  |  [optional] |
|**embeddingCount** | **Integer** |  |  [optional] |
|**warnings** | **List&lt;String&gt;** |  |  [optional] |



