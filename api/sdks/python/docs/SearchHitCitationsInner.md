# SearchHitCitationsInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**source_kind** | **str** |  | [optional] 
**source_id** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.search_hit_citations_inner import SearchHitCitationsInner

# TODO update the JSON string below
json = "{}"
# create an instance of SearchHitCitationsInner from a JSON string
search_hit_citations_inner_instance = SearchHitCitationsInner.from_json(json)
# print the JSON string representation of the object
print(SearchHitCitationsInner.to_json())

# convert the object into a dict
search_hit_citations_inner_dict = search_hit_citations_inner_instance.to_dict()
# create an instance of SearchHitCitationsInner from a dict
search_hit_citations_inner_from_dict = SearchHitCitationsInner.from_dict(search_hit_citations_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


