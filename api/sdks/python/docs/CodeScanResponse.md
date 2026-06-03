# CodeScanResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | 
**skipped** | **bool** |  | 
**project** | **str** |  | 
**files** | **int** |  | 
**inspected** | **int** |  | 

## Example

```python
from aimee_kb.models.code_scan_response import CodeScanResponse

# TODO update the JSON string below
json = "{}"
# create an instance of CodeScanResponse from a JSON string
code_scan_response_instance = CodeScanResponse.from_json(json)
# print the JSON string representation of the object
print(CodeScanResponse.to_json())

# convert the object into a dict
code_scan_response_dict = code_scan_response_instance.to_dict()
# create an instance of CodeScanResponse from a dict
code_scan_response_from_dict = CodeScanResponse.from_dict(code_scan_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


