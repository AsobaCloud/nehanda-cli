# IngestRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**workspace** | **str** | Workspace root, workspace name, or \&quot;all\&quot;. | [optional] 
**embedding_command** | **str** |  | [optional] 
**force** | **bool** |  | [optional] [default to False]

## Example

```python
from aimee_kb.models.ingest_request import IngestRequest

# TODO update the JSON string below
json = "{}"
# create an instance of IngestRequest from a JSON string
ingest_request_instance = IngestRequest.from_json(json)
# print the JSON string representation of the object
print(IngestRequest.to_json())

# convert the object into a dict
ingest_request_dict = ingest_request_instance.to_dict()
# create an instance of IngestRequest from a dict
ingest_request_from_dict = IngestRequest.from_dict(ingest_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


