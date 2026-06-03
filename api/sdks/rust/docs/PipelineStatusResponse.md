# PipelineStatusResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**state** | Option<**State**> |  (enum: idle, running, failed) | [optional]
**queue_depth** | Option<**i32**> |  | [optional]
**active_jobs** | Option<**Vec<serde_json::Value>**> |  | [optional]
**queue** | Option<[**models::PipelineStatusResponseQueue**](PipelineStatusResponseQueue.md)> |  | [optional]

[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


