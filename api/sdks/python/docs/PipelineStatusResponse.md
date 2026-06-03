# PipelineStatusResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**state** | **str** |  | [optional] 
**queue_depth** | **int** |  | [optional] 
**active_jobs** | **List[object]** |  | [optional] 
**queue** | [**PipelineStatusResponseQueue**](PipelineStatusResponseQueue.md) |  | [optional] 

## Example

```python
from aimee_kb.models.pipeline_status_response import PipelineStatusResponse

# TODO update the JSON string below
json = "{}"
# create an instance of PipelineStatusResponse from a JSON string
pipeline_status_response_instance = PipelineStatusResponse.from_json(json)
# print the JSON string representation of the object
print(PipelineStatusResponse.to_json())

# convert the object into a dict
pipeline_status_response_dict = pipeline_status_response_instance.to_dict()
# create an instance of PipelineStatusResponse from a dict
pipeline_status_response_from_dict = PipelineStatusResponse.from_dict(pipeline_status_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


