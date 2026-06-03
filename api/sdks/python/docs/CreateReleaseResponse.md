# CreateReleaseResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**release_id** | **int** |  | [optional] 
**state** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.create_release_response import CreateReleaseResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CreateReleaseResponse from a JSON string
create_release_response_instance = CreateReleaseResponse.from_json(json)
# print the JSON string representation of the object
print(CreateReleaseResponse.to_json())

# convert the object into a dict
create_release_response_dict = create_release_response_instance.to_dict()
# create an instance of CreateReleaseResponse from a dict
create_release_response_from_dict = CreateReleaseResponse.from_dict(create_release_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


