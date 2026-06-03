# DocIngestResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**doc_id** | **int** |  | [optional] 
**state** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.doc_ingest_response import DocIngestResponse

# TODO update the JSON string below
json = "{}"
# create an instance of DocIngestResponse from a JSON string
doc_ingest_response_instance = DocIngestResponse.from_json(json)
# print the JSON string representation of the object
print(DocIngestResponse.to_json())

# convert the object into a dict
doc_ingest_response_dict = doc_ingest_response_instance.to_dict()
# create an instance of DocIngestResponse from a dict
doc_ingest_response_from_dict = DocIngestResponse.from_dict(doc_ingest_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


