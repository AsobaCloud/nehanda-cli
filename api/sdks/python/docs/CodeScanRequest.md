# CodeScanRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**project** | **str** |  | 
**root_path** | **str** |  | 
**force** | **bool** |  | [optional] [default to False]

## Example

```python
from aimee_kb.models.code_scan_request import CodeScanRequest

# TODO update the JSON string below
json = "{}"
# create an instance of CodeScanRequest from a JSON string
code_scan_request_instance = CodeScanRequest.from_json(json)
# print the JSON string representation of the object
print(CodeScanRequest.to_json())

# convert the object into a dict
code_scan_request_dict = code_scan_request_instance.to_dict()
# create an instance of CodeScanRequest from a dict
code_scan_request_from_dict = CodeScanRequest.from_dict(code_scan_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


