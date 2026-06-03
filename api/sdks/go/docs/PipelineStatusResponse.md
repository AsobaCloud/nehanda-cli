# PipelineStatusResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**State** | Pointer to **string** |  | [optional] 
**QueueDepth** | Pointer to **int32** |  | [optional] 
**ActiveJobs** | Pointer to **[]map[string]interface{}** |  | [optional] 
**Queue** | Pointer to [**PipelineStatusResponseQueue**](PipelineStatusResponseQueue.md) |  | [optional] 

## Methods

### NewPipelineStatusResponse

`func NewPipelineStatusResponse() *PipelineStatusResponse`

NewPipelineStatusResponse instantiates a new PipelineStatusResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewPipelineStatusResponseWithDefaults

`func NewPipelineStatusResponseWithDefaults() *PipelineStatusResponse`

NewPipelineStatusResponseWithDefaults instantiates a new PipelineStatusResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetState

`func (o *PipelineStatusResponse) GetState() string`

GetState returns the State field if non-nil, zero value otherwise.

### GetStateOk

`func (o *PipelineStatusResponse) GetStateOk() (*string, bool)`

GetStateOk returns a tuple with the State field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetState

`func (o *PipelineStatusResponse) SetState(v string)`

SetState sets State field to given value.

### HasState

`func (o *PipelineStatusResponse) HasState() bool`

HasState returns a boolean if a field has been set.

### GetQueueDepth

`func (o *PipelineStatusResponse) GetQueueDepth() int32`

GetQueueDepth returns the QueueDepth field if non-nil, zero value otherwise.

### GetQueueDepthOk

`func (o *PipelineStatusResponse) GetQueueDepthOk() (*int32, bool)`

GetQueueDepthOk returns a tuple with the QueueDepth field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetQueueDepth

`func (o *PipelineStatusResponse) SetQueueDepth(v int32)`

SetQueueDepth sets QueueDepth field to given value.

### HasQueueDepth

`func (o *PipelineStatusResponse) HasQueueDepth() bool`

HasQueueDepth returns a boolean if a field has been set.

### GetActiveJobs

`func (o *PipelineStatusResponse) GetActiveJobs() []map[string]interface{}`

GetActiveJobs returns the ActiveJobs field if non-nil, zero value otherwise.

### GetActiveJobsOk

`func (o *PipelineStatusResponse) GetActiveJobsOk() (*[]map[string]interface{}, bool)`

GetActiveJobsOk returns a tuple with the ActiveJobs field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetActiveJobs

`func (o *PipelineStatusResponse) SetActiveJobs(v []map[string]interface{})`

SetActiveJobs sets ActiveJobs field to given value.

### HasActiveJobs

`func (o *PipelineStatusResponse) HasActiveJobs() bool`

HasActiveJobs returns a boolean if a field has been set.

### GetQueue

`func (o *PipelineStatusResponse) GetQueue() PipelineStatusResponseQueue`

GetQueue returns the Queue field if non-nil, zero value otherwise.

### GetQueueOk

`func (o *PipelineStatusResponse) GetQueueOk() (*PipelineStatusResponseQueue, bool)`

GetQueueOk returns a tuple with the Queue field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetQueue

`func (o *PipelineStatusResponse) SetQueue(v PipelineStatusResponseQueue)`

SetQueue sets Queue field to given value.

### HasQueue

`func (o *PipelineStatusResponse) HasQueue() bool`

HasQueue returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


