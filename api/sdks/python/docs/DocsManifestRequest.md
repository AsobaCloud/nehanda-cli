# DocsManifestRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**scope** | **str** |  | [optional] [default to 'global']
**docs** | [**List[DocsManifestRequestDocsInner]**](DocsManifestRequestDocsInner.md) |  | 

## Example

```python
from aimee_kb.models.docs_manifest_request import DocsManifestRequest

# TODO update the JSON string below
json = "{}"
# create an instance of DocsManifestRequest from a JSON string
docs_manifest_request_instance = DocsManifestRequest.from_json(json)
# print the JSON string representation of the object
print(DocsManifestRequest.to_json())

# convert the object into a dict
docs_manifest_request_dict = docs_manifest_request_instance.to_dict()
# create an instance of DocsManifestRequest from a dict
docs_manifest_request_from_dict = DocsManifestRequest.from_dict(docs_manifest_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


