# EntitySearchRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**query** | **str** |  | 
**limit** | **int** |  | [optional] [default to 10]
**cursor** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.entity_search_request import EntitySearchRequest

# TODO update the JSON string below
json = "{}"
# create an instance of EntitySearchRequest from a JSON string
entity_search_request_instance = EntitySearchRequest.from_json(json)
# print the JSON string representation of the object
print(EntitySearchRequest.to_json())

# convert the object into a dict
entity_search_request_dict = entity_search_request_instance.to_dict()
# create an instance of EntitySearchRequest from a dict
entity_search_request_from_dict = EntitySearchRequest.from_dict(entity_search_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


