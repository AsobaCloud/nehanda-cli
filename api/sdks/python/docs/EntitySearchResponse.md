# EntitySearchResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**entities** | [**List[EntitySearchResponseEntitiesInner]**](EntitySearchResponseEntitiesInner.md) |  | [optional] 
**next_cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.entity_search_response import EntitySearchResponse

# TODO update the JSON string below
json = "{}"
# create an instance of EntitySearchResponse from a JSON string
entity_search_response_instance = EntitySearchResponse.from_json(json)
# print the JSON string representation of the object
print(EntitySearchResponse.to_json())

# convert the object into a dict
entity_search_response_dict = entity_search_response_instance.to_dict()
# create an instance of EntitySearchResponse from a dict
entity_search_response_from_dict = EntitySearchResponse.from_dict(entity_search_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


