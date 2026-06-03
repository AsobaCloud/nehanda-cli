# CodeSearchResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**hits** | [**List[CodeSearchHit]**](CodeSearchHit.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_search_response import CodeSearchResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeSearchResponse from a JSON string
code_search_response_instance = CodeSearchResponse.from_json(json)
# print the JSON string representation of the object
print(CodeSearchResponse.to_json())

# convert the object into a dict
code_search_response_dict = code_search_response_instance.to_dict()
# create an instance of CodeSearchResponse from a dict
code_search_response_from_dict = CodeSearchResponse.from_dict(code_search_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


