# CodeProjectStatsResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**project** | **str** |  | [optional] 
**files** | **int** |  | [optional] 
**definitions** | **int** |  | [optional] 
**langs** | [**List[CodeProjectLanguage]**](CodeProjectLanguage.md) |  | [optional] 

## Example

```python
from aimee_kb.models.code_project_stats_response import CodeProjectStatsResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeProjectStatsResponse from a JSON string
code_project_stats_response_instance = CodeProjectStatsResponse.from_json(json)
# print the JSON string representation of the object
print(CodeProjectStatsResponse.to_json())

# convert the object into a dict
code_project_stats_response_dict = code_project_stats_response_instance.to_dict()
# create an instance of CodeProjectStatsResponse from a dict
code_project_stats_response_from_dict = CodeProjectStatsResponse.from_dict(code_project_stats_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


