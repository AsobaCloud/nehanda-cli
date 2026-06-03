# DocsManifestRequestDocsInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**doc_key** | **str** |  | [optional] 
**path** | **str** |  | [optional] 
**content_hash** | **str** |  | 
**scope** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.docs_manifest_request_docs_inner import DocsManifestRequestDocsInner

# TODO update the JSON string below
json = "{}"
# create an instance of DocsManifestRequestDocsInner from a JSON string
docs_manifest_request_docs_inner_instance = DocsManifestRequestDocsInner.from_json(json)
# print the JSON string representation of the object
print(DocsManifestRequestDocsInner.to_json())

# convert the object into a dict
docs_manifest_request_docs_inner_dict = docs_manifest_request_docs_inner_instance.to_dict()
# create an instance of DocsManifestRequestDocsInner from a dict
docs_manifest_request_docs_inner_from_dict = DocsManifestRequestDocsInner.from_dict(docs_manifest_request_docs_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


