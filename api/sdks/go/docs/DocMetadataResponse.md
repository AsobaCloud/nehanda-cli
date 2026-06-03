# DocMetadataResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Id** | Pointer to **int64** |  | [optional] 
**Filename** | Pointer to **string** |  | [optional] 
**ContentHash** | Pointer to **string** |  | [optional] 
**Converter** | Pointer to **string** |  | [optional] 
**ConverterVersion** | Pointer to **string** |  | [optional] 
**Scope** | Pointer to **string** |  | [optional] 
**State** | Pointer to **string** |  | [optional] 
**ReviewNeeded** | Pointer to **bool** |  | [optional] 
**CreatedAt** | Pointer to **time.Time** |  | [optional] 

## Methods

### NewDocMetadataResponse

`func NewDocMetadataResponse() *DocMetadataResponse`

NewDocMetadataResponse instantiates a new DocMetadataResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewDocMetadataResponseWithDefaults

`func NewDocMetadataResponseWithDefaults() *DocMetadataResponse`

NewDocMetadataResponseWithDefaults instantiates a new DocMetadataResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetId

`func (o *DocMetadataResponse) GetId() int64`

GetId returns the Id field if non-nil, zero value otherwise.

### GetIdOk

`func (o *DocMetadataResponse) GetIdOk() (*int64, bool)`

GetIdOk returns a tuple with the Id field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetId

`func (o *DocMetadataResponse) SetId(v int64)`

SetId sets Id field to given value.

### HasId

`func (o *DocMetadataResponse) HasId() bool`

HasId returns a boolean if a field has been set.

### GetFilename

`func (o *DocMetadataResponse) GetFilename() string`

GetFilename returns the Filename field if non-nil, zero value otherwise.

### GetFilenameOk

`func (o *DocMetadataResponse) GetFilenameOk() (*string, bool)`

GetFilenameOk returns a tuple with the Filename field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilename

`func (o *DocMetadataResponse) SetFilename(v string)`

SetFilename sets Filename field to given value.

### HasFilename

`func (o *DocMetadataResponse) HasFilename() bool`

HasFilename returns a boolean if a field has been set.

### GetContentHash

`func (o *DocMetadataResponse) GetContentHash() string`

GetContentHash returns the ContentHash field if non-nil, zero value otherwise.

### GetContentHashOk

`func (o *DocMetadataResponse) GetContentHashOk() (*string, bool)`

GetContentHashOk returns a tuple with the ContentHash field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetContentHash

`func (o *DocMetadataResponse) SetContentHash(v string)`

SetContentHash sets ContentHash field to given value.

### HasContentHash

`func (o *DocMetadataResponse) HasContentHash() bool`

HasContentHash returns a boolean if a field has been set.

### GetConverter

`func (o *DocMetadataResponse) GetConverter() string`

GetConverter returns the Converter field if non-nil, zero value otherwise.

### GetConverterOk

`func (o *DocMetadataResponse) GetConverterOk() (*string, bool)`

GetConverterOk returns a tuple with the Converter field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetConverter

`func (o *DocMetadataResponse) SetConverter(v string)`

SetConverter sets Converter field to given value.

### HasConverter

`func (o *DocMetadataResponse) HasConverter() bool`

HasConverter returns a boolean if a field has been set.

### GetConverterVersion

`func (o *DocMetadataResponse) GetConverterVersion() string`

GetConverterVersion returns the ConverterVersion field if non-nil, zero value otherwise.

### GetConverterVersionOk

`func (o *DocMetadataResponse) GetConverterVersionOk() (*string, bool)`

GetConverterVersionOk returns a tuple with the ConverterVersion field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetConverterVersion

`func (o *DocMetadataResponse) SetConverterVersion(v string)`

SetConverterVersion sets ConverterVersion field to given value.

### HasConverterVersion

`func (o *DocMetadataResponse) HasConverterVersion() bool`

HasConverterVersion returns a boolean if a field has been set.

### GetScope

`func (o *DocMetadataResponse) GetScope() string`

GetScope returns the Scope field if non-nil, zero value otherwise.

### GetScopeOk

`func (o *DocMetadataResponse) GetScopeOk() (*string, bool)`

GetScopeOk returns a tuple with the Scope field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScope

`func (o *DocMetadataResponse) SetScope(v string)`

SetScope sets Scope field to given value.

### HasScope

`func (o *DocMetadataResponse) HasScope() bool`

HasScope returns a boolean if a field has been set.

### GetState

`func (o *DocMetadataResponse) GetState() string`

GetState returns the State field if non-nil, zero value otherwise.

### GetStateOk

`func (o *DocMetadataResponse) GetStateOk() (*string, bool)`

GetStateOk returns a tuple with the State field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetState

`func (o *DocMetadataResponse) SetState(v string)`

SetState sets State field to given value.

### HasState

`func (o *DocMetadataResponse) HasState() bool`

HasState returns a boolean if a field has been set.

### GetReviewNeeded

`func (o *DocMetadataResponse) GetReviewNeeded() bool`

GetReviewNeeded returns the ReviewNeeded field if non-nil, zero value otherwise.

### GetReviewNeededOk

`func (o *DocMetadataResponse) GetReviewNeededOk() (*bool, bool)`

GetReviewNeededOk returns a tuple with the ReviewNeeded field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetReviewNeeded

`func (o *DocMetadataResponse) SetReviewNeeded(v bool)`

SetReviewNeeded sets ReviewNeeded field to given value.

### HasReviewNeeded

`func (o *DocMetadataResponse) HasReviewNeeded() bool`

HasReviewNeeded returns a boolean if a field has been set.

### GetCreatedAt

`func (o *DocMetadataResponse) GetCreatedAt() time.Time`

GetCreatedAt returns the CreatedAt field if non-nil, zero value otherwise.

### GetCreatedAtOk

`func (o *DocMetadataResponse) GetCreatedAtOk() (*time.Time, bool)`

GetCreatedAtOk returns a tuple with the CreatedAt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCreatedAt

`func (o *DocMetadataResponse) SetCreatedAt(v time.Time)`

SetCreatedAt sets CreatedAt field to given value.

### HasCreatedAt

`func (o *DocMetadataResponse) HasCreatedAt() bool`

HasCreatedAt returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


