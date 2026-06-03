# SearchResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**hits** | [**List[SearchHit]**](SearchHit.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 
**total_hits** | **int** |  | [optional] 
**fusion_mode_used** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.search_response import SearchResponse

# TODO update the JSON string below
json = "{}"
# create an instance of SearchResponse from a JSON string
search_response_instance = SearchResponse.from_json(json)
# print the JSON string representation of the object
print(SearchResponse.to_json())

# convert the object into a dict
search_response_dict = search_response_instance.to_dict()
# create an instance of SearchResponse from a dict
search_response_from_dict = SearchResponse.from_dict(search_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


