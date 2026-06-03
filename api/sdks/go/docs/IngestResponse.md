# IngestResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**ProjectsQueued** | Pointer to **int32** |  | [optional] 
**Message** | Pointer to **string** |  | [optional] 

## Methods

### NewIngestResponse

`func NewIngestResponse() *IngestResponse`

NewIngestResponse instantiates a new IngestResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewIngestResponseWithDefaults

`func NewIngestResponseWithDefaults() *IngestResponse`

NewIngestResponseWithDefaults instantiates a new IngestResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *IngestResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *IngestResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *IngestResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *IngestResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProjectsQueued

`func (o *IngestResponse) GetProjectsQueued() int32`

GetProjectsQueued returns the ProjectsQueued field if non-nil, zero value otherwise.

### GetProjectsQueuedOk

`func (o *IngestResponse) GetProjectsQueuedOk() (*int32, bool)`

GetProjectsQueuedOk returns a tuple with the ProjectsQueued field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProjectsQueued

`func (o *IngestResponse) SetProjectsQueued(v int32)`

SetProjectsQueued sets ProjectsQueued field to given value.

### HasProjectsQueued

`func (o *IngestResponse) HasProjectsQueued() bool`

HasProjectsQueued returns a boolean if a field has been set.

### GetMessage

`func (o *IngestResponse) GetMessage() string`

GetMessage returns the Message field if non-nil, zero value otherwise.

### GetMessageOk

`func (o *IngestResponse) GetMessageOk() (*string, bool)`

GetMessageOk returns a tuple with the Message field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetMessage

`func (o *IngestResponse) SetMessage(v string)`

SetMessage sets Message field to given value.

### HasMessage

`func (o *IngestResponse) HasMessage() bool`

HasMessage returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


