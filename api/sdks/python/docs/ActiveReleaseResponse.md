# ActiveReleaseResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**release_id** | **int** |  | [optional] 
**name** | **str** |  | [optional] 
**state** | **str** |  | [optional] 
**promoted_at** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.active_release_response import ActiveReleaseResponse

# TODO update the JSON string below
json = "{}"
# create an instance of ActiveReleaseResponse from a JSON string
active_release_response_instance = ActiveReleaseResponse.from_json(json)
# print the JSON string representation of the object
print(ActiveReleaseResponse.to_json())

# convert the object into a dict
active_release_response_dict = active_release_response_instance.to_dict()
# create an instance of ActiveReleaseResponse from a dict
active_release_response_from_dict = ActiveReleaseResponse.from_dict(active_release_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


