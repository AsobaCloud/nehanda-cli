# DocsManifestResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**missing** | [**List[DocsManifestResponseMissingInner]**](DocsManifestResponseMissingInner.md) |  | 
**total** | **int** |  | 
**present** | **int** |  | 
**missing_count** | **int** |  | 

## Example

```python
from aimee_kb.models.docs_manifest_response import DocsManifestResponse

# TODO update the JSON string below
json = "{}"
# create an instance of DocsManifestResponse from a JSON string
docs_manifest_response_instance = DocsManifestResponse.from_json(json)
# print the JSON string representation of the object
print(DocsManifestResponse.to_json())

# convert the object into a dict
docs_manifest_response_dict = docs_manifest_response_instance.to_dict()
# create an instance of DocsManifestResponse from a dict
docs_manifest_response_from_dict = DocsManifestResponse.from_dict(docs_manifest_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


