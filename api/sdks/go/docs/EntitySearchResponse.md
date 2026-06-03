# EntitySearchResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Entities** | Pointer to [**[]EntitySearchResponseEntitiesInner**](EntitySearchResponseEntitiesInner.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 

## Methods

### NewEntitySearchResponse

`func NewEntitySearchResponse() *EntitySearchResponse`

NewEntitySearchResponse instantiates a new EntitySearchResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewEntitySearchResponseWithDefaults

`func NewEntitySearchResponseWithDefaults() *EntitySearchResponse`

NewEntitySearchResponseWithDefaults instantiates a new EntitySearchResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetEntities

`func (o *EntitySearchResponse) GetEntities() []EntitySearchResponseEntitiesInner`

GetEntities returns the Entities field if non-nil, zero value otherwise.

### GetEntitiesOk

`func (o *EntitySearchResponse) GetEntitiesOk() (*[]EntitySearchResponseEntitiesInner, bool)`

GetEntitiesOk returns a tuple with the Entities field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEntities

`func (o *EntitySearchResponse) SetEntities(v []EntitySearchResponseEntitiesInner)`

SetEntities sets Entities field to given value.

### HasEntities

`func (o *EntitySearchResponse) HasEntities() bool`

HasEntities returns a boolean if a field has been set.

### GetNextCursor

`func (o *EntitySearchResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *EntitySearchResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *EntitySearchResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *EntitySearchResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


