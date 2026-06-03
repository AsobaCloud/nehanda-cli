# ArtifactLinksResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**artifact_id** | **str** |  | [optional] 
**links** | [**List[ArtifactLinksResponseLinksInner]**](ArtifactLinksResponseLinksInner.md) |  | [optional] 

## Example

```python
from aimee_kb.models.artifact_links_response import ArtifactLinksResponse

# TODO update the JSON string below
json = "{}"
# create an instance of ArtifactLinksResponse from a JSON string
artifact_links_response_instance = ArtifactLinksResponse.from_json(json)
# print the JSON string representation of the object
print(ArtifactLinksResponse.to_json())

# convert the object into a dict
artifact_links_response_dict = artifact_links_response_instance.to_dict()
# create an instance of ArtifactLinksResponse from a dict
artifact_links_response_from_dict = ArtifactLinksResponse.from_dict(artifact_links_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


