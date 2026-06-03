# IngestStatusResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**queue** | [**IngestStatusResponseQueue**](IngestStatusResponseQueue.md) |  | [optional] 
**workers** | [**IngestStatusResponseWorkers**](IngestStatusResponseWorkers.md) |  | [optional] 
**recent** | [**List[IngestStatusResponseRecentInner]**](IngestStatusResponseRecentInner.md) |  | [optional] 

## Example

```python
from aimee_kb.models.ingest_status_response import IngestStatusResponse

# TODO update the JSON string below
json = "{}"
# create an instance of IngestStatusResponse from a JSON string
ingest_status_response_instance = IngestStatusResponse.from_json(json)
# print the JSON string representation of the object
print(IngestStatusResponse.to_json())

# convert the object into a dict
ingest_status_response_dict = ingest_status_response_instance.to_dict()
# create an instance of IngestStatusResponse from a dict
ingest_status_response_from_dict = IngestStatusResponse.from_dict(ingest_status_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


