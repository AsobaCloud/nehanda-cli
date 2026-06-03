# IngestStatusResponseQueue


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**pending** | **int** |  | [optional] 
**running** | **int** |  | [optional] 
**done_last_24h** | **int** |  | [optional] 
**failed_last_24h** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.ingest_status_response_queue import IngestStatusResponseQueue

# TODO update the JSON string below
json = "{}"
# create an instance of IngestStatusResponseQueue from a JSON string
ingest_status_response_queue_instance = IngestStatusResponseQueue.from_json(json)
# print the JSON string representation of the object
print(IngestStatusResponseQueue.to_json())

# convert the object into a dict
ingest_status_response_queue_dict = ingest_status_response_queue_instance.to_dict()
# create an instance of IngestStatusResponseQueue from a dict
ingest_status_response_queue_from_dict = IngestStatusResponseQueue.from_dict(ingest_status_response_queue_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


