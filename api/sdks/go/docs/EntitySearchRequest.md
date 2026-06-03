# EntitySearchRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Query** | **string** |  | 
**Limit** | Pointer to **int32** |  | [optional] [default to 10]
**Cursor** | Pointer to **string** |  | [optional] 

## Methods

### NewEntitySearchRequest

`func NewEntitySearchRequest(query string, ) *EntitySearchRequest`

NewEntitySearchRequest instantiates a new EntitySearchRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewEntitySearchRequestWithDefaults

`func NewEntitySearchRequestWithDefaults() *EntitySearchRequest`

NewEntitySearchRequestWithDefaults instantiates a new EntitySearchRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetQuery

`func (o *EntitySearchRequest) GetQuery() string`

GetQuery returns the Query field if non-nil, zero value otherwise.

### GetQueryOk

`func (o *EntitySearchRequest) GetQueryOk() (*string, bool)`

GetQueryOk returns a tuple with the Query field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetQuery

`func (o *EntitySearchRequest) SetQuery(v string)`

SetQuery sets Query field to given value.


### GetLimit

`func (o *EntitySearchRequest) GetLimit() int32`

GetLimit returns the Limit field if non-nil, zero value otherwise.

### GetLimitOk

`func (o *EntitySearchRequest) GetLimitOk() (*int32, bool)`

GetLimitOk returns a tuple with the Limit field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetLimit

`func (o *EntitySearchRequest) SetLimit(v int32)`

SetLimit sets Limit field to given value.

### HasLimit

`func (o *EntitySearchRequest) HasLimit() bool`

HasLimit returns a boolean if a field has been set.

### GetCursor

`func (o *EntitySearchRequest) GetCursor() string`

GetCursor returns the Cursor field if non-nil, zero value otherwise.

### GetCursorOk

`func (o *EntitySearchRequest) GetCursorOk() (*string, bool)`

GetCursorOk returns a tuple with the Cursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCursor

`func (o *EntitySearchRequest) SetCursor(v string)`

SetCursor sets Cursor field to given value.

### HasCursor

`func (o *EntitySearchRequest) HasCursor() bool`

HasCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


