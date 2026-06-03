# WorkersResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Configured** | Pointer to **int32** |  | [optional] 
**Slots** | Pointer to **[]map[string]interface{}** |  | [optional] 
**Threads** | Pointer to **[]map[string]interface{}** |  | [optional] 
**Background** | Pointer to **[]map[string]interface{}** |  | [optional] 

## Methods

### NewWorkersResponse

`func NewWorkersResponse() *WorkersResponse`

NewWorkersResponse instantiates a new WorkersResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewWorkersResponseWithDefaults

`func NewWorkersResponseWithDefaults() *WorkersResponse`

NewWorkersResponseWithDefaults instantiates a new WorkersResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *WorkersResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *WorkersResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *WorkersResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *WorkersResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetConfigured

`func (o *WorkersResponse) GetConfigured() int32`

GetConfigured returns the Configured field if non-nil, zero value otherwise.

### GetConfiguredOk

`func (o *WorkersResponse) GetConfiguredOk() (*int32, bool)`

GetConfiguredOk returns a tuple with the Configured field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetConfigured

`func (o *WorkersResponse) SetConfigured(v int32)`

SetConfigured sets Configured field to given value.

### HasConfigured

`func (o *WorkersResponse) HasConfigured() bool`

HasConfigured returns a boolean if a field has been set.

### GetSlots

`func (o *WorkersResponse) GetSlots() []map[string]interface{}`

GetSlots returns the Slots field if non-nil, zero value otherwise.

### GetSlotsOk

`func (o *WorkersResponse) GetSlotsOk() (*[]map[string]interface{}, bool)`

GetSlotsOk returns a tuple with the Slots field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetSlots

`func (o *WorkersResponse) SetSlots(v []map[string]interface{})`

SetSlots sets Slots field to given value.

### HasSlots

`func (o *WorkersResponse) HasSlots() bool`

HasSlots returns a boolean if a field has been set.

### GetThreads

`func (o *WorkersResponse) GetThreads() []map[string]interface{}`

GetThreads returns the Threads field if non-nil, zero value otherwise.

### GetThreadsOk

`func (o *WorkersResponse) GetThreadsOk() (*[]map[string]interface{}, bool)`

GetThreadsOk returns a tuple with the Threads field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetThreads

`func (o *WorkersResponse) SetThreads(v []map[string]interface{})`

SetThreads sets Threads field to given value.

### HasThreads

`func (o *WorkersResponse) HasThreads() bool`

HasThreads returns a boolean if a field has been set.

### GetBackground

`func (o *WorkersResponse) GetBackground() []map[string]interface{}`

GetBackground returns the Background field if non-nil, zero value otherwise.

### GetBackgroundOk

`func (o *WorkersResponse) GetBackgroundOk() (*[]map[string]interface{}, bool)`

GetBackgroundOk returns a tuple with the Background field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetBackground

`func (o *WorkersResponse) SetBackground(v []map[string]interface{})`

SetBackground sets Background field to given value.

### HasBackground

`func (o *WorkersResponse) HasBackground() bool`

HasBackground returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


