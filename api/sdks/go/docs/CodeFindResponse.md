# CodeFindResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Hits** | Pointer to [**[]CodeFindHit**](CodeFindHit.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 

## Methods

### NewCodeFindResponse

`func NewCodeFindResponse() *CodeFindResponse`

NewCodeFindResponse instantiates a new CodeFindResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeFindResponseWithDefaults

`func NewCodeFindResponseWithDefaults() *CodeFindResponse`

NewCodeFindResponseWithDefaults instantiates a new CodeFindResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetHits

`func (o *CodeFindResponse) GetHits() []CodeFindHit`

GetHits returns the Hits field if non-nil, zero value otherwise.

### GetHitsOk

`func (o *CodeFindResponse) GetHitsOk() (*[]CodeFindHit, bool)`

GetHitsOk returns a tuple with the Hits field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetHits

`func (o *CodeFindResponse) SetHits(v []CodeFindHit)`

SetHits sets Hits field to given value.

### HasHits

`func (o *CodeFindResponse) HasHits() bool`

HasHits returns a boolean if a field has been set.

### GetNextCursor

`func (o *CodeFindResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *CodeFindResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *CodeFindResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *CodeFindResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


