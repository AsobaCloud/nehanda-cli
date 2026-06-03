# IngestResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**projects_queued** | **int** |  | [optional] 
**message** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.ingest_response import IngestResponse

# TODO update the JSON string below
json = "{}"
# create an instance of IngestResponse from a JSON string
ingest_response_instance = IngestResponse.from_json(json)
# print the JSON string representation of the object
print(IngestResponse.to_json())

# convert the object into a dict
ingest_response_dict = ingest_response_instance.to_dict()
# create an instance of IngestResponse from a dict
ingest_response_from_dict = IngestResponse.from_dict(ingest_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


