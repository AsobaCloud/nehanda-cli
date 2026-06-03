# CodeBuildResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**project** | **str** |  | [optional] 
**files_scanned** | **int** |  | [optional] 
**files_indexed** | **int** |  | [optional] 
**files_skipped** | **int** |  | [optional] 
**files_removed** | **int** |  | [optional] 
**chunks_added** | **int** |  | [optional] 
**chunks_removed** | **int** |  | [optional] 
**embeddings_added** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.code_build_response import CodeBuildResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeBuildResponse from a JSON string
code_build_response_instance = CodeBuildResponse.from_json(json)
# print the JSON string representation of the object
print(CodeBuildResponse.to_json())

# convert the object into a dict
code_build_response_dict = code_build_response_instance.to_dict()
# create an instance of CodeBuildResponse from a dict
code_build_response_from_dict = CodeBuildResponse.from_dict(code_build_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


