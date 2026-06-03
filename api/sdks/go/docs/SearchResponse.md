# SearchResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Hits** | Pointer to [**[]SearchHit**](SearchHit.md) |  | [optional] 
**NextCursor** | Pointer to **string** |  | [optional] 
**TotalHits** | Pointer to **int32** |  | [optional] 
**FusionModeUsed** | Pointer to **string** |  | [optional] 

## Methods

### NewSearchResponse

`func NewSearchResponse() *SearchResponse`

NewSearchResponse instantiates a new SearchResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewSearchResponseWithDefaults

`func NewSearchResponseWithDefaults() *SearchResponse`

NewSearchResponseWithDefaults instantiates a new SearchResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetHits

`func (o *SearchResponse) GetHits() []SearchHit`

GetHits returns the Hits field if non-nil, zero value otherwise.

### GetHitsOk

`func (o *SearchResponse) GetHitsOk() (*[]SearchHit, bool)`

GetHitsOk returns a tuple with the Hits field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetHits

`func (o *SearchResponse) SetHits(v []SearchHit)`

SetHits sets Hits field to given value.

### HasHits

`func (o *SearchResponse) HasHits() bool`

HasHits returns a boolean if a field has been set.

### GetNextCursor

`func (o *SearchResponse) GetNextCursor() string`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *SearchResponse) GetNextCursorOk() (*string, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *SearchResponse) SetNextCursor(v string)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *SearchResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.

### GetTotalHits

`func (o *SearchResponse) GetTotalHits() int32`

GetTotalHits returns the TotalHits field if non-nil, zero value otherwise.

### GetTotalHitsOk

`func (o *SearchResponse) GetTotalHitsOk() (*int32, bool)`

GetTotalHitsOk returns a tuple with the TotalHits field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetTotalHits

`func (o *SearchResponse) SetTotalHits(v int32)`

SetTotalHits sets TotalHits field to given value.

### HasTotalHits

`func (o *SearchResponse) HasTotalHits() bool`

HasTotalHits returns a boolean if a field has been set.

### GetFusionModeUsed

`func (o *SearchResponse) GetFusionModeUsed() string`

GetFusionModeUsed returns the FusionModeUsed field if non-nil, zero value otherwise.

### GetFusionModeUsedOk

`func (o *SearchResponse) GetFusionModeUsedOk() (*string, bool)`

GetFusionModeUsedOk returns a tuple with the FusionModeUsed field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFusionModeUsed

`func (o *SearchResponse) SetFusionModeUsed(v string)`

SetFusionModeUsed sets FusionModeUsed field to given value.

### HasFusionModeUsed

`func (o *SearchResponse) HasFusionModeUsed() bool`

HasFusionModeUsed returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


