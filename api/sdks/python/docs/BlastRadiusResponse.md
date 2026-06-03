# BlastRadiusResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**file** | **str** |  | [optional] 
**dependents** | **List[str]** |  | [optional] 
**dependent_count** | **int** |  | [optional] 
**dependencies** | **List[str]** |  | [optional] 
**dependency_count** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.blast_radius_response import BlastRadiusResponse

# TODO update the JSON string below
json = "{}"
# create an instance of BlastRadiusResponse from a JSON string
blast_radius_response_instance = BlastRadiusResponse.from_json(json)
# print the JSON string representation of the object
print(BlastRadiusResponse.to_json())

# convert the object into a dict
blast_radius_response_dict = blast_radius_response_instance.to_dict()
# create an instance of BlastRadiusResponse from a dict
blast_radius_response_from_dict = BlastRadiusResponse.from_dict(blast_radius_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


