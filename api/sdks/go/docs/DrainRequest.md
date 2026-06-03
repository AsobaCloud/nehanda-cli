# DrainRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**EmbeddingCommand** | Pointer to **string** |  | [optional] 
**Timeout** | Pointer to **int32** |  | [optional] [default to 0]

## Methods

### NewDrainRequest

`func NewDrainRequest() *DrainRequest`

NewDrainRequest instantiates a new DrainRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewDrainRequestWithDefaults

`func NewDrainRequestWithDefaults() *DrainRequest`

NewDrainRequestWithDefaults instantiates a new DrainRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetEmbeddingCommand

`func (o *DrainRequest) GetEmbeddingCommand() string`

GetEmbeddingCommand returns the EmbeddingCommand field if non-nil, zero value otherwise.

### GetEmbeddingCommandOk

`func (o *DrainRequest) GetEmbeddingCommandOk() (*string, bool)`

GetEmbeddingCommandOk returns a tuple with the EmbeddingCommand field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingCommand

`func (o *DrainRequest) SetEmbeddingCommand(v string)`

SetEmbeddingCommand sets EmbeddingCommand field to given value.

### HasEmbeddingCommand

`func (o *DrainRequest) HasEmbeddingCommand() bool`

HasEmbeddingCommand returns a boolean if a field has been set.

### GetTimeout

`func (o *DrainRequest) GetTimeout() int32`

GetTimeout returns the Timeout field if non-nil, zero value otherwise.

### GetTimeoutOk

`func (o *DrainRequest) GetTimeoutOk() (*int32, bool)`

GetTimeoutOk returns a tuple with the Timeout field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetTimeout

`func (o *DrainRequest) SetTimeout(v int32)`

SetTimeout sets Timeout field to given value.

### HasTimeout

`func (o *DrainRequest) HasTimeout() bool`

HasTimeout returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


