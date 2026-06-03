# CodeFindResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**hits** | [**List[CodeFindHit]**](CodeFindHit.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_find_response import CodeFindResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeFindResponse from a JSON string
code_find_response_instance = CodeFindResponse.from_json(json)
# print the JSON string representation of the object
print(CodeFindResponse.to_json())

# convert the object into a dict
code_find_response_dict = code_find_response_instance.to_dict()
# create an instance of CodeFindResponse from a dict
code_find_response_from_dict = CodeFindResponse.from_dict(code_find_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


