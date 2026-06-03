# CodeUpdateRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Path** | **string** |  | 
**Project** | **string** |  | 
**EmbeddingCommand** | Pointer to **string** |  | [optional] 

## Methods

### NewCodeUpdateRequest

`func NewCodeUpdateRequest(path string, project string, ) *CodeUpdateRequest`

NewCodeUpdateRequest instantiates a new CodeUpdateRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeUpdateRequestWithDefaults

`func NewCodeUpdateRequestWithDefaults() *CodeUpdateRequest`

NewCodeUpdateRequestWithDefaults instantiates a new CodeUpdateRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetPath

`func (o *CodeUpdateRequest) GetPath() string`

GetPath returns the Path field if non-nil, zero value otherwise.

### GetPathOk

`func (o *CodeUpdateRequest) GetPathOk() (*string, bool)`

GetPathOk returns a tuple with the Path field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPath

`func (o *CodeUpdateRequest) SetPath(v string)`

SetPath sets Path field to given value.


### GetProject

`func (o *CodeUpdateRequest) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeUpdateRequest) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeUpdateRequest) SetProject(v string)`

SetProject sets Project field to given value.


### GetEmbeddingCommand

`func (o *CodeUpdateRequest) GetEmbeddingCommand() string`

GetEmbeddingCommand returns the EmbeddingCommand field if non-nil, zero value otherwise.

### GetEmbeddingCommandOk

`func (o *CodeUpdateRequest) GetEmbeddingCommandOk() (*string, bool)`

GetEmbeddingCommandOk returns a tuple with the EmbeddingCommand field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingCommand

`func (o *CodeUpdateRequest) SetEmbeddingCommand(v string)`

SetEmbeddingCommand sets EmbeddingCommand field to given value.

### HasEmbeddingCommand

`func (o *CodeUpdateRequest) HasEmbeddingCommand() bool`

HasEmbeddingCommand returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


