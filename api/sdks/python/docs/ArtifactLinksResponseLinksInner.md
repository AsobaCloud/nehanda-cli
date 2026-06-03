# ArtifactLinksResponseLinksInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**to_id** | **str** |  | [optional] 
**link_kind** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.artifact_links_response_links_inner import ArtifactLinksResponseLinksInner

# TODO update the JSON string below
json = "{}"
# create an instance of ArtifactLinksResponseLinksInner from a JSON string
artifact_links_response_links_inner_instance = ArtifactLinksResponseLinksInner.from_json(json)
# print the JSON string representation of the object
print(ArtifactLinksResponseLinksInner.to_json())

# convert the object into a dict
artifact_links_response_links_inner_dict = artifact_links_response_links_inner_instance.to_dict()
# create an instance of ArtifactLinksResponseLinksInner from a dict
artifact_links_response_links_inner_from_dict = ArtifactLinksResponseLinksInner.from_dict(artifact_links_response_links_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


