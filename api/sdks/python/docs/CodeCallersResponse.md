# CodeCallersResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**hits** | [**List[CodeCallerHit]**](CodeCallerHit.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_callers_response import CodeCallersResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeCallersResponse from a JSON string
code_callers_response_instance = CodeCallersResponse.from_json(json)
# print the JSON string representation of the object
print(CodeCallersResponse.to_json())

# convert the object into a dict
code_callers_response_dict = code_callers_response_instance.to_dict()
# create an instance of CodeCallersResponse from a dict
code_callers_response_from_dict = CodeCallersResponse.from_dict(code_callers_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


