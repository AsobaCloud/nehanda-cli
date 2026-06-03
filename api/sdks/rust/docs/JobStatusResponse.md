# JobStatusResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **i64** |  | 
**kind** | **String** |  | 
**document_id** | **i64** |  | 
**project** | **String** |  | 
**status** | **Status** |  (enum: pending, running, done, failed) | 
**attempts** | **i32** |  | 
**last_error** | Option<**String**> |  | [optional]
**claimed_by** | Option<**String**> |  | [optional]
**claimed_at** | Option<**String**> |  | [optional]
**created_at** | Option<**String**> |  | [optional]
**updated_at** | Option<**String**> |  | [optional]

[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


