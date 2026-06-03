# CodeCallersResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Hits** | Pointer to [**[]CodeCallerHit**](CodeCallerHit.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 

## Methods

### NewCodeCallersResponse

`func NewCodeCallersResponse() *CodeCallersResponse`

NewCodeCallersResponse instantiates a new CodeCallersResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeCallersResponseWithDefaults

`func NewCodeCallersResponseWithDefaults() *CodeCallersResponse`

NewCodeCallersResponseWithDefaults instantiates a new CodeCallersResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeCallersResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeCallersResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeCallersResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeCallersResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetHits

`func (o *CodeCallersResponse) GetHits() []CodeCallerHit`

GetHits returns the Hits field if non-nil, zero value otherwise.

### GetHitsOk

`func (o *CodeCallersResponse) GetHitsOk() (*[]CodeCallerHit, bool)`

GetHitsOk returns a tuple with the Hits field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetHits

`func (o *CodeCallersResponse) SetHits(v []CodeCallerHit)`

SetHits sets Hits field to given value.

### HasHits

`func (o *CodeCallersResponse) HasHits() bool`

HasHits returns a boolean if a field has been set.

### GetNextCursor

`func (o *CodeCallersResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *CodeCallersResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *CodeCallersResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *CodeCallersResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


