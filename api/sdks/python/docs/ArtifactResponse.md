# ArtifactResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | [optional] 
**kind** | **str** |  | [optional] 
**state** | **str** |  | [optional] 
**scope_kind** | **str** |  | [optional] 
**scope_id** | **str** |  | [optional] 
**confidence** | **float** |  | [optional] 
**payload** | **object** |  | [optional] 
**citations** | [**List[SearchHitCitationsInner]**](SearchHitCitationsInner.md) |  | [optional] 
**updated_at** | **datetime** |  | [optional] 

## Example

```python
from aimee_kb.models.artifact_response import ArtifactResponse

# TODO update the JSON string below
json = "{}"
# create an instance of ArtifactResponse from a JSON string
artifact_response_instance = ArtifactResponse.from_json(json)
# print the JSON string representation of the object
print(ArtifactResponse.to_json())

# convert the object into a dict
artifact_response_dict = artifact_response_instance.to_dict()
# create an instance of ArtifactResponse from a dict
artifact_response_from_dict = ArtifactResponse.from_dict(artifact_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


