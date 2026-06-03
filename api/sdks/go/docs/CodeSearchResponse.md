# CodeSearchResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Hits** | Pointer to [**[]CodeSearchHit**](CodeSearchHit.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 

## Methods

### NewCodeSearchResponse

`func NewCodeSearchResponse() *CodeSearchResponse`

NewCodeSearchResponse instantiates a new CodeSearchResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeSearchResponseWithDefaults

`func NewCodeSearchResponseWithDefaults() *CodeSearchResponse`

NewCodeSearchResponseWithDefaults instantiates a new CodeSearchResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeSearchResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeSearchResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeSearchResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeSearchResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetHits

`func (o *CodeSearchResponse) GetHits() []CodeSearchHit`

GetHits returns the Hits field if non-nil, zero value otherwise.

### GetHitsOk

`func (o *CodeSearchResponse) GetHitsOk() (*[]CodeSearchHit, bool)`

GetHitsOk returns a tuple with the Hits field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetHits

`func (o *CodeSearchResponse) SetHits(v []CodeSearchHit)`

SetHits sets Hits field to given value.

### HasHits

`func (o *CodeSearchResponse) HasHits() bool`

HasHits returns a boolean if a field has been set.

### GetNextCursor

`func (o *CodeSearchResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *CodeSearchResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *CodeSearchResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *CodeSearchResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


