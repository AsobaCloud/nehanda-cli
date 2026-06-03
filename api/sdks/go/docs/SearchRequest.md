# SearchRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Query** | **string** |  | 
**Project** | Pointer to **string** |  | [optional] 
**ScopeKind** | Pointer to **string** |  | [optional] 
**ScopeId** | Pointer to **string** |  | [optional] 
**MaxResults** | Pointer to **int32** |  | [optional] [default to 10]
**FusionMode** | Pointer to **string** |  | [optional] 
**Cursor** | Pointer to **string** |  | [optional] 

## Methods

### NewSearchRequest

`func NewSearchRequest(query string, ) *SearchRequest`

NewSearchRequest instantiates a new SearchRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewSearchRequestWithDefaults

`func NewSearchRequestWithDefaults() *SearchRequest`

NewSearchRequestWithDefaults instantiates a new SearchRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetQuery

`func (o *SearchRequest) GetQuery() string`

GetQuery returns the Query field if non-nil, zero value otherwise.

### GetQueryOk

`func (o *SearchRequest) GetQueryOk() (*string, bool)`

GetQueryOk returns a tuple with the Query field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetQuery

`func (o *SearchRequest) SetQuery(v string)`

SetQuery sets Query field to given value.


### GetProject

`func (o *SearchRequest) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *SearchRequest) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *SearchRequest) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *SearchRequest) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetScopeKind

`func (o *SearchRequest) GetScopeKind() string`

GetScopeKind returns the ScopeKind field if non-nil, zero value otherwise.

### GetScopeKindOk

`func (o *SearchRequest) GetScopeKindOk() (*string, bool)`

GetScopeKindOk returns a tuple with the ScopeKind field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScopeKind

`func (o *SearchRequest) SetScopeKind(v string)`

SetScopeKind sets ScopeKind field to given value.

### HasScopeKind

`func (o *SearchRequest) HasScopeKind() bool`

HasScopeKind returns a boolean if a field has been set.

### GetScopeId

`func (o *SearchRequest) GetScopeId() string`

GetScopeId returns the ScopeId field if non-nil, zero value otherwise.

### GetScopeIdOk

`func (o *SearchRequest) GetScopeIdOk() (*string, bool)`

GetScopeIdOk returns a tuple with the ScopeId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScopeId

`func (o *SearchRequest) SetScopeId(v string)`

SetScopeId sets ScopeId field to given value.

### HasScopeId

`func (o *SearchRequest) HasScopeId() bool`

HasScopeId returns a boolean if a field has been set.

### GetMaxResults

`func (o *SearchRequest) GetMaxResults() int32`

GetMaxResults returns the MaxResults field if non-nil, zero value otherwise.

### GetMaxResultsOk

`func (o *SearchRequest) GetMaxResultsOk() (*int32, bool)`

GetMaxResultsOk returns a tuple with the MaxResults field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetMaxResults

`func (o *SearchRequest) SetMaxResults(v int32)`

SetMaxResults sets MaxResults field to given value.

### HasMaxResults

`func (o *SearchRequest) HasMaxResults() bool`

HasMaxResults returns a boolean if a field has been set.

### GetFusionMode

`func (o *SearchRequest) GetFusionMode() string`

GetFusionMode returns the FusionMode field if non-nil, zero value otherwise.

### GetFusionModeOk

`func (o *SearchRequest) GetFusionModeOk() (*string, bool)`

GetFusionModeOk returns a tuple with the FusionMode field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFusionMode

`func (o *SearchRequest) SetFusionMode(v string)`

SetFusionMode sets FusionMode field to given value.

### HasFusionMode

`func (o *SearchRequest) HasFusionMode() bool`

HasFusionMode returns a boolean if a field has been set.

### GetCursor

`func (o *SearchRequest) GetCursor() string`

GetCursor returns the Cursor field if non-nil, zero value otherwise.

### GetCursorOk

`func (o *SearchRequest) GetCursorOk() (*string, bool)`

GetCursorOk returns a tuple with the Cursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCursor

`func (o *SearchRequest) SetCursor(v string)`

SetCursor sets Cursor field to given value.

### HasCursor

`func (o *SearchRequest) HasCursor() bool`

HasCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


