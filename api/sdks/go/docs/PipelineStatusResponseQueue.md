# PipelineStatusResponseQueue

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Pending** | Pointer to **int32** |  | [optional] 
**Running** | Pointer to **int32** |  | [optional] 
**Done** | Pointer to **int32** |  | [optional] 
**Failed** | Pointer to **int32** |  | [optional] 
**Total** | Pointer to **int32** |  | [optional] 

## Methods

### NewPipelineStatusResponseQueue

`func NewPipelineStatusResponseQueue() *PipelineStatusResponseQueue`

NewPipelineStatusResponseQueue instantiates a new PipelineStatusResponseQueue object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewPipelineStatusResponseQueueWithDefaults

`func NewPipelineStatusResponseQueueWithDefaults() *PipelineStatusResponseQueue`

NewPipelineStatusResponseQueueWithDefaults instantiates a new PipelineStatusResponseQueue object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetPending

`func (o *PipelineStatusResponseQueue) GetPending() int32`

GetPending returns the Pending field if non-nil, zero value otherwise.

### GetPendingOk

`func (o *PipelineStatusResponseQueue) GetPendingOk() (*int32, bool)`

GetPendingOk returns a tuple with the Pending field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPending

`func (o *PipelineStatusResponseQueue) SetPending(v int32)`

SetPending sets Pending field to given value.

### HasPending

`func (o *PipelineStatusResponseQueue) HasPending() bool`

HasPending returns a boolean if a field has been set.

### GetRunning

`func (o *PipelineStatusResponseQueue) GetRunning() int32`

GetRunning returns the Running field if non-nil, zero value otherwise.

### GetRunningOk

`func (o *PipelineStatusResponseQueue) GetRunningOk() (*int32, bool)`

GetRunningOk returns a tuple with the Running field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRunning

`func (o *PipelineStatusResponseQueue) SetRunning(v int32)`

SetRunning sets Running field to given value.

### HasRunning

`func (o *PipelineStatusResponseQueue) HasRunning() bool`

HasRunning returns a boolean if a field has been set.

### GetDone

`func (o *PipelineStatusResponseQueue) GetDone() int32`

GetDone returns the Done field if non-nil, zero value otherwise.

### GetDoneOk

`func (o *PipelineStatusResponseQueue) GetDoneOk() (*int32, bool)`

GetDoneOk returns a tuple with the Done field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDone

`func (o *PipelineStatusResponseQueue) SetDone(v int32)`

SetDone sets Done field to given value.

### HasDone

`func (o *PipelineStatusResponseQueue) HasDone() bool`

HasDone returns a boolean if a field has been set.

### GetFailed

`func (o *PipelineStatusResponseQueue) GetFailed() int32`

GetFailed returns the Failed field if non-nil, zero value otherwise.

### GetFailedOk

`func (o *PipelineStatusResponseQueue) GetFailedOk() (*int32, bool)`

GetFailedOk returns a tuple with the Failed field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFailed

`func (o *PipelineStatusResponseQueue) SetFailed(v int32)`

SetFailed sets Failed field to given value.

### HasFailed

`func (o *PipelineStatusResponseQueue) HasFailed() bool`

HasFailed returns a boolean if a field has been set.

### GetTotal

`func (o *PipelineStatusResponseQueue) GetTotal() int32`

GetTotal returns the Total field if non-nil, zero value otherwise.

### GetTotalOk

`func (o *PipelineStatusResponseQueue) GetTotalOk() (*int32, bool)`

GetTotalOk returns a tuple with the Total field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetTotal

`func (o *PipelineStatusResponseQueue) SetTotal(v int32)`

SetTotal sets Total field to given value.

### HasTotal

`func (o *PipelineStatusResponseQueue) HasTotal() bool`

HasTotal returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


