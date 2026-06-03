# MaintenanceClearResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Project** | Pointer to **string** |  | [optional] 
**ChunksDeleted** | Pointer to **int32** |  | [optional] 

## Methods

### NewMaintenanceClearResponse

`func NewMaintenanceClearResponse() *MaintenanceClearResponse`

NewMaintenanceClearResponse instantiates a new MaintenanceClearResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewMaintenanceClearResponseWithDefaults

`func NewMaintenanceClearResponseWithDefaults() *MaintenanceClearResponse`

NewMaintenanceClearResponseWithDefaults instantiates a new MaintenanceClearResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *MaintenanceClearResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *MaintenanceClearResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *MaintenanceClearResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *MaintenanceClearResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProject

`func (o *MaintenanceClearResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *MaintenanceClearResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *MaintenanceClearResponse) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *MaintenanceClearResponse) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetChunksDeleted

`func (o *MaintenanceClearResponse) GetChunksDeleted() int32`

GetChunksDeleted returns the ChunksDeleted field if non-nil, zero value otherwise.

### GetChunksDeletedOk

`func (o *MaintenanceClearResponse) GetChunksDeletedOk() (*int32, bool)`

GetChunksDeletedOk returns a tuple with the ChunksDeleted field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunksDeleted

`func (o *MaintenanceClearResponse) SetChunksDeleted(v int32)`

SetChunksDeleted sets ChunksDeleted field to given value.

### HasChunksDeleted

`func (o *MaintenanceClearResponse) HasChunksDeleted() bool`

HasChunksDeleted returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


