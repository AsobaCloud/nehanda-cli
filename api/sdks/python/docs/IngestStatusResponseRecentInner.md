# IngestStatusResponseRecentInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**project** | **str** |  | [optional] 
**status** | **str** |  | [optional] 
**completed_at** | **str** |  | [optional] 
**files_indexed** | **int** |  | [optional] 
**chunks_added** | **int** |  | [optional] 
**error** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.ingest_status_response_recent_inner import IngestStatusResponseRecentInner

# TODO update the JSON string below
json = "{}"
# create an instance of IngestStatusResponseRecentInner from a JSON string
ingest_status_response_recent_inner_instance = IngestStatusResponseRecentInner.from_json(json)
# print the JSON string representation of the object
print(IngestStatusResponseRecentInner.to_json())

# convert the object into a dict
ingest_status_response_recent_inner_dict = ingest_status_response_recent_inner_instance.to_dict()
# create an instance of IngestStatusResponseRecentInner from a dict
ingest_status_response_recent_inner_from_dict = IngestStatusResponseRecentInner.from_dict(ingest_status_response_recent_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


