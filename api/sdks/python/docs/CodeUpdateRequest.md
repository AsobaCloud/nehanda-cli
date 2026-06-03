# CodeUpdateRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**path** | **str** |  | 
**project** | **str** |  | 
**embedding_command** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_update_request import CodeUpdateRequest

# TODO update the JSON string below
json = "{}"
# create an instance of CodeUpdateRequest from a JSON string
code_update_request_instance = CodeUpdateRequest.from_json(json)
# print the JSON string representation of the object
print(CodeUpdateRequest.to_json())

# convert the object into a dict
code_update_request_dict = code_update_request_instance.to_dict()
# create an instance of CodeUpdateRequest from a dict
code_update_request_from_dict = CodeUpdateRequest.from_dict(code_update_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


