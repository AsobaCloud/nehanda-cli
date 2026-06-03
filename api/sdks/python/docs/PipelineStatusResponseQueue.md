# PipelineStatusResponseQueue


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**pending** | **int** |  | [optional] 
**running** | **int** |  | [optional] 
**done** | **int** |  | [optional] 
**failed** | **int** |  | [optional] 
**total** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.pipeline_status_response_queue import PipelineStatusResponseQueue

# TODO update the JSON string below
json = "{}"
# create an instance of PipelineStatusResponseQueue from a JSON string
pipeline_status_response_queue_instance = PipelineStatusResponseQueue.from_json(json)
# print the JSON string representation of the object
print(PipelineStatusResponseQueue.to_json())

# convert the object into a dict
pipeline_status_response_queue_dict = pipeline_status_response_queue_instance.to_dict()
# create an instance of PipelineStatusResponseQueue from a dict
pipeline_status_response_queue_from_dict = PipelineStatusResponseQueue.from_dict(pipeline_status_response_queue_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


