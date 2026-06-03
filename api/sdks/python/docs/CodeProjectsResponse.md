# CodeProjectsResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**projects** | [**List[CodeProject]**](CodeProject.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_projects_response import CodeProjectsResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeProjectsResponse from a JSON string
code_projects_response_instance = CodeProjectsResponse.from_json(json)
# print the JSON string representation of the object
print(CodeProjectsResponse.to_json())

# convert the object into a dict
code_projects_response_dict = code_projects_response_instance.to_dict()
# create an instance of CodeProjectsResponse from a dict
code_projects_response_from_dict = CodeProjectsResponse.from_dict(code_projects_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


