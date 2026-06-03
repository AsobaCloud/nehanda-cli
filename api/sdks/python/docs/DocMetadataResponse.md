# DocMetadataResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **int** |  | [optional] 
**filename** | **str** |  | [optional] 
**content_hash** | **str** |  | [optional] 
**converter** | **str** |  | [optional] 
**converter_version** | **str** |  | [optional] 
**scope** | **str** |  | [optional] 
**state** | **str** |  | [optional] 
**review_needed** | **bool** |  | [optional] 
**created_at** | **datetime** |  | [optional] 

## Example

```python
from aimee_kb.models.doc_metadata_response import DocMetadataResponse

# TODO update the JSON string below
json = "{}"
# create an instance of DocMetadataResponse from a JSON string
doc_metadata_response_instance = DocMetadataResponse.from_json(json)
# print the JSON string representation of the object
print(DocMetadataResponse.to_json())

# convert the object into a dict
doc_metadata_response_dict = doc_metadata_response_instance.to_dict()
# create an instance of DocMetadataResponse from a dict
doc_metadata_response_from_dict = DocMetadataResponse.from_dict(doc_metadata_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


