# CodeCallerHit


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**project** | **str** |  | [optional] 
**file_path** | **str** |  | [optional] 
**caller** | **str** |  | [optional] 
**line** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.code_caller_hit import CodeCallerHit

# TODO update the JSON string below
json = "{}"
# create an instance of CodeCallerHit from a JSON string
code_caller_hit_instance = CodeCallerHit.from_json(json)
# print the JSON string representation of the object
print(CodeCallerHit.to_json())

# convert the object into a dict
code_caller_hit_dict = code_caller_hit_instance.to_dict()
# create an instance of CodeCallerHit from a dict
code_caller_hit_from_dict = CodeCallerHit.from_dict(code_caller_hit_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


