# EntityProfileResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**entity** | **str** |  | [optional] 
**kind** | **str** |  | [optional] 
**summary** | **str** |  | [optional] 
**facts** | **List[str]** |  | [optional] 
**tags** | **List[str]** |  | [optional] 
**updated_at** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.entity_profile_response import EntityProfileResponse

# TODO update the JSON string below
json = "{}"
# create an instance of EntityProfileResponse from a JSON string
entity_profile_response_instance = EntityProfileResponse.from_json(json)
# print the JSON string representation of the object
print(EntityProfileResponse.to_json())

# convert the object into a dict
entity_profile_response_dict = entity_profile_response_instance.to_dict()
# create an instance of EntityProfileResponse from a dict
entity_profile_response_from_dict = EntityProfileResponse.from_dict(entity_profile_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


