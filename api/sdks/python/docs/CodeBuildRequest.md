# CodeBuildRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**path** | **str** |  | 
**project** | **str** |  | 
**embedding_command** | **str** |  | [optional] 
**force** | **bool** |  | [optional] [default to False]

## Example

```python
from aimee_kb.models.code_build_request import CodeBuildRequest

# TODO update the JSON string below
json = "{}"
# create an instance of CodeBuildRequest from a JSON string
code_build_request_instance = CodeBuildRequest.from_json(json)
# print the JSON string representation of the object
print(CodeBuildRequest.to_json())

# convert the object into a dict
code_build_request_dict = code_build_request_instance.to_dict()
# create an instance of CodeBuildRequest from a dict
code_build_request_from_dict = CodeBuildRequest.from_dict(code_build_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


