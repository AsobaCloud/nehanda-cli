# IngestStatusResponseWorkers


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**configured** | **int** |  | [optional] 
**active** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.ingest_status_response_workers import IngestStatusResponseWorkers

# TODO update the JSON string below
json = "{}"
# create an instance of IngestStatusResponseWorkers from a JSON string
ingest_status_response_workers_instance = IngestStatusResponseWorkers.from_json(json)
# print the JSON string representation of the object
print(IngestStatusResponseWorkers.to_json())

# convert the object into a dict
ingest_status_response_workers_dict = ingest_status_response_workers_instance.to_dict()
# create an instance of IngestStatusResponseWorkers from a dict
ingest_status_response_workers_from_dict = IngestStatusResponseWorkers.from_dict(ingest_status_response_workers_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


