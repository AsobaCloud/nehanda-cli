# IngestStatusResponseQueue

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Pending** | Pointer to **int32** |  | [optional] 
**Running** | Pointer to **int32** |  | [optional] 
**DoneLast24h** | Pointer to **int32** |  | [optional] 
**FailedLast24h** | Pointer to **int32** |  | [optional] 

## Methods

### NewIngestStatusResponseQueue

`func NewIngestStatusResponseQueue() *IngestStatusResponseQueue`

NewIngestStatusResponseQueue instantiates a new IngestStatusResponseQueue object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewIngestStatusResponseQueueWithDefaults

`func NewIngestStatusResponseQueueWithDefaults() *IngestStatusResponseQueue`

NewIngestStatusResponseQueueWithDefaults instantiates a new IngestStatusResponseQueue object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetPending

`func (o *IngestStatusResponseQueue) GetPending() int32`

GetPending returns the Pending field if non-nil, zero value otherwise.

### GetPendingOk

`func (o *IngestStatusResponseQueue) GetPendingOk() (*int32, bool)`

GetPendingOk returns a tuple with the Pending field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPending

`func (o *IngestStatusResponseQueue) SetPending(v int32)`

SetPending sets Pending field to given value.

### HasPending

`func (o *IngestStatusResponseQueue) HasPending() bool`

HasPending returns a boolean if a field has been set.

### GetRunning

`func (o *IngestStatusResponseQueue) GetRunning() int32`

GetRunning returns the Running field if non-nil, zero value otherwise.

### GetRunningOk

`func (o *IngestStatusResponseQueue) GetRunningOk() (*int32, bool)`

GetRunningOk returns a tuple with the Running field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRunning

`func (o *IngestStatusResponseQueue) SetRunning(v int32)`

SetRunning sets Running field to given value.

### HasRunning

`func (o *IngestStatusResponseQueue) HasRunning() bool`

HasRunning returns a boolean if a field has been set.

### GetDoneLast24h

`func (o *IngestStatusResponseQueue) GetDoneLast24h() int32`

GetDoneLast24h returns the DoneLast24h field if non-nil, zero value otherwise.

### GetDoneLast24hOk

`func (o *IngestStatusResponseQueue) GetDoneLast24hOk() (*int32, bool)`

GetDoneLast24hOk returns a tuple with the DoneLast24h field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDoneLast24h

`func (o *IngestStatusResponseQueue) SetDoneLast24h(v int32)`

SetDoneLast24h sets DoneLast24h field to given value.

### HasDoneLast24h

`func (o *IngestStatusResponseQueue) HasDoneLast24h() bool`

HasDoneLast24h returns a boolean if a field has been set.

### GetFailedLast24h

`func (o *IngestStatusResponseQueue) GetFailedLast24h() int32`

GetFailedLast24h returns the FailedLast24h field if non-nil, zero value otherwise.

### GetFailedLast24hOk

`func (o *IngestStatusResponseQueue) GetFailedLast24hOk() (*int32, bool)`

GetFailedLast24hOk returns a tuple with the FailedLast24h field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFailedLast24h

`func (o *IngestStatusResponseQueue) SetFailedLast24h(v int32)`

SetFailedLast24h sets FailedLast24h field to given value.

### HasFailedLast24h

`func (o *IngestStatusResponseQueue) HasFailedLast24h() bool`

HasFailedLast24h returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


