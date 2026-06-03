# CodeDefinition


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**name** | **str** |  | [optional] 
**kind** | **str** |  | [optional] 
**line** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.code_definition import CodeDefinition

# TODO update the JSON string below
json = "{}"
# create an instance of CodeDefinition from a JSON string
code_definition_instance = CodeDefinition.from_json(json)
# print the JSON string representation of the object
print(CodeDefinition.to_json())

# convert the object into a dict
code_definition_dict = code_definition_instance.to_dict()
# create an instance of CodeDefinition from a dict
code_definition_from_dict = CodeDefinition.from_dict(code_definition_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


