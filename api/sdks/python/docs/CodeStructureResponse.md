# CodeStructureResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**definitions** | [**List[CodeDefinition]**](CodeDefinition.md) |  | [optional] 

## Example

```python
from aimee_kb.models.code_structure_response import CodeStructureResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeStructureResponse from a JSON string
code_structure_response_instance = CodeStructureResponse.from_json(json)
# print the JSON string representation of the object
print(CodeStructureResponse.to_json())

# convert the object into a dict
code_structure_response_dict = code_structure_response_instance.to_dict()
# create an instance of CodeStructureResponse from a dict
code_structure_response_from_dict = CodeStructureResponse.from_dict(code_structure_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


