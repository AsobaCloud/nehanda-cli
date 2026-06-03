# EntitySearchResponseEntitiesInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**entity** | **str** |  | [optional] 
**kind** | **str** |  | [optional] 
**summary** | **str** |  | [optional] 
**score** | **float** |  | [optional] 

## Example

```python
from aimee_kb.models.entity_search_response_entities_inner import EntitySearchResponseEntitiesInner

# TODO update the JSON string below
json = "{}"
# create an instance of EntitySearchResponseEntitiesInner from a JSON string
entity_search_response_entities_inner_instance = EntitySearchResponseEntitiesInner.from_json(json)
# print the JSON string representation of the object
print(EntitySearchResponseEntitiesInner.to_json())

# convert the object into a dict
entity_search_response_entities_inner_dict = entity_search_response_entities_inner_instance.to_dict()
# create an instance of EntitySearchResponseEntitiesInner from a dict
entity_search_response_entities_inner_from_dict = EntitySearchResponseEntitiesInner.from_dict(entity_search_response_entities_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


