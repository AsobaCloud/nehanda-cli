# IngestStatusResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Queue** | Pointer to [**IngestStatusResponseQueue**](IngestStatusResponseQueue.md) |  | [optional] 
**Workers** | Pointer to [**IngestStatusResponseWorkers**](IngestStatusResponseWorkers.md) |  | [optional] 
**Recent** | Pointer to [**[]IngestStatusResponseRecentInner**](IngestStatusResponseRecentInner.md) |  | [optional] 

## Methods

### NewIngestStatusResponse

`func NewIngestStatusResponse() *IngestStatusResponse`

NewIngestStatusResponse instantiates a new IngestStatusResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewIngestStatusResponseWithDefaults

`func NewIngestStatusResponseWithDefaults() *IngestStatusResponse`

NewIngestStatusResponseWithDefaults instantiates a new IngestStatusResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *IngestStatusResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *IngestStatusResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *IngestStatusResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *IngestStatusResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetQueue

`func (o *IngestStatusResponse) GetQueue() IngestStatusResponseQueue`

GetQueue returns the Queue field if non-nil, zero value otherwise.

### GetQueueOk

`func (o *IngestStatusResponse) GetQueueOk() (*IngestStatusResponseQueue, bool)`

GetQueueOk returns a tuple with the Queue field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetQueue

`func (o *IngestStatusResponse) SetQueue(v IngestStatusResponseQueue)`

SetQueue sets Queue field to given value.

### HasQueue

`func (o *IngestStatusResponse) HasQueue() bool`

HasQueue returns a boolean if a field has been set.

### GetWorkers

`func (o *IngestStatusResponse) GetWorkers() IngestStatusResponseWorkers`

GetWorkers returns the Workers field if non-nil, zero value otherwise.

### GetWorkersOk

`func (o *IngestStatusResponse) GetWorkersOk() (*IngestStatusResponseWorkers, bool)`

GetWorkersOk returns a tuple with the Workers field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetWorkers

`func (o *IngestStatusResponse) SetWorkers(v IngestStatusResponseWorkers)`

SetWorkers sets Workers field to given value.

### HasWorkers

`func (o *IngestStatusResponse) HasWorkers() bool`

HasWorkers returns a boolean if a field has been set.

### GetRecent

`func (o *IngestStatusResponse) GetRecent() []IngestStatusResponseRecentInner`

GetRecent returns the Recent field if non-nil, zero value otherwise.

### GetRecentOk

`func (o *IngestStatusResponse) GetRecentOk() (*[]IngestStatusResponseRecentInner, bool)`

GetRecentOk returns a tuple with the Recent field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRecent

`func (o *IngestStatusResponse) SetRecent(v []IngestStatusResponseRecentInner)`

SetRecent sets Recent field to given value.

### HasRecent

`func (o *IngestStatusResponse) HasRecent() bool`

HasRecent returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


