# CapabilitiesResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**capabilities** | **List[str]** |  | 
**version** | **str** |  | 

## Example

```python
from aimee_kb.models.capabilities_response import CapabilitiesResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CapabilitiesResponse from a JSON string
capabilities_response_instance = CapabilitiesResponse.from_json(json)
# print the JSON string representation of the object
print(CapabilitiesResponse.to_json())

# convert the object into a dict
capabilities_response_dict = capabilities_response_instance.to_dict()
# create an instance of CapabilitiesResponse from a dict
capabilities_response_from_dict = CapabilitiesResponse.from_dict(capabilities_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


