# CodeProject


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**name** | **str** |  | [optional] 
**root** | **str** |  | [optional] 
**scanned_at** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_project import CodeProject

# TODO update the JSON string below
json = "{}"
# create an instance of CodeProject from a JSON string
code_project_instance = CodeProject.from_json(json)
# print the JSON string representation of the object
print(CodeProject.to_json())

# convert the object into a dict
code_project_dict = code_project_instance.to_dict()
# create an instance of CodeProject from a dict
code_project_from_dict = CodeProject.from_dict(code_project_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


