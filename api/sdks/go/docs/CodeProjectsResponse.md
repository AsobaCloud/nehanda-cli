# CodeProjectsResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Projects** | Pointer to [**[]CodeProject**](CodeProject.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 

## Methods

### NewCodeProjectsResponse

`func NewCodeProjectsResponse() *CodeProjectsResponse`

NewCodeProjectsResponse instantiates a new CodeProjectsResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeProjectsResponseWithDefaults

`func NewCodeProjectsResponseWithDefaults() *CodeProjectsResponse`

NewCodeProjectsResponseWithDefaults instantiates a new CodeProjectsResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeProjectsResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeProjectsResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeProjectsResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeProjectsResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProjects

`func (o *CodeProjectsResponse) GetProjects() []CodeProject`

GetProjects returns the Projects field if non-nil, zero value otherwise.

### GetProjectsOk

`func (o *CodeProjectsResponse) GetProjectsOk() (*[]CodeProject, bool)`

GetProjectsOk returns a tuple with the Projects field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProjects

`func (o *CodeProjectsResponse) SetProjects(v []CodeProject)`

SetProjects sets Projects field to given value.

### HasProjects

`func (o *CodeProjectsResponse) HasProjects() bool`

HasProjects returns a boolean if a field has been set.

### GetNextCursor

`func (o *CodeProjectsResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *CodeProjectsResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *CodeProjectsResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *CodeProjectsResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


