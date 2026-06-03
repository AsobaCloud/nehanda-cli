# HealthResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** | ok when healthy; a non-ok string when degraded | 
**db2_ok** | **bool** | DB2 (Postgres) reachable | [optional] 
**db2_kb_tables_ok** | **bool** | required kb tables present | [optional] 
**pgvec_ok** | **bool** | pgvector extension available | [optional] 
**pgvec_collection_ok** | **bool** |  | [optional] 
**embed_ok** | **bool** |  | [optional] 
**embed_command** | **str** |  | [optional] 
**chunk_count** | **int** |  | [optional] 
**embedding_count** | **int** |  | [optional] 
**warnings** | **List[str]** |  | [optional] 

## Example

```python
from aimee_kb.models.health_response import HealthResponse

# TODO update the JSON string below
json = "{}"
# create an instance of HealthResponse from a JSON string
health_response_instance = HealthResponse.from_json(json)
# print the JSON string representation of the object
print(HealthResponse.to_json())

# convert the object into a dict
health_response_dict = health_response_instance.to_dict()
# create an instance of HealthResponse from a dict
health_response_from_dict = HealthResponse.from_dict(health_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


