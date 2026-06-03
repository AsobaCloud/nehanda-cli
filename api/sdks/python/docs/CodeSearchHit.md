# CodeSearchHit


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**project** | **str** |  | [optional] 
**file_path** | **str** |  | [optional] 
**snippet** | **str** |  | [optional] 
**rank** | **float** |  | [optional] 

## Example

```python
from aimee_kb.models.code_search_hit import CodeSearchHit

# TODO update the JSON string below
json = "{}"
# create an instance of CodeSearchHit from a JSON string
code_search_hit_instance = CodeSearchHit.from_json(json)
# print the JSON string representation of the object
print(CodeSearchHit.to_json())

# convert the object into a dict
code_search_hit_dict = code_search_hit_instance.to_dict()
# create an instance of CodeSearchHit from a dict
code_search_hit_from_dict = CodeSearchHit.from_dict(code_search_hit_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


