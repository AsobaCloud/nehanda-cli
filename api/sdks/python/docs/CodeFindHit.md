# CodeFindHit


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**project** | **str** |  | [optional] 
**file_path** | **str** |  | [optional] 
**line** | **int** |  | [optional] 
**kind** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.code_find_hit import CodeFindHit

# TODO update the JSON string below
json = "{}"
# create an instance of CodeFindHit from a JSON string
code_find_hit_instance = CodeFindHit.from_json(json)
# print the JSON string representation of the object
print(CodeFindHit.to_json())

# convert the object into a dict
code_find_hit_dict = code_find_hit_instance.to_dict()
# create an instance of CodeFindHit from a dict
code_find_hit_from_dict = CodeFindHit.from_dict(code_find_hit_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


