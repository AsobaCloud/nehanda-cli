# CodeBuildRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Path** | **string** |  | 
**Project** | **string** |  | 
**EmbeddingCommand** | Pointer to **string** |  | [optional] 
**Force** | Pointer to **bool** |  | [optional] [default to false]

## Methods

### NewCodeBuildRequest

`func NewCodeBuildRequest(path string, project string, ) *CodeBuildRequest`

NewCodeBuildRequest instantiates a new CodeBuildRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeBuildRequestWithDefaults

`func NewCodeBuildRequestWithDefaults() *CodeBuildRequest`

NewCodeBuildRequestWithDefaults instantiates a new CodeBuildRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetPath

`func (o *CodeBuildRequest) GetPath() string`

GetPath returns the Path field if non-nil, zero value otherwise.

### GetPathOk

`func (o *CodeBuildRequest) GetPathOk() (*string, bool)`

GetPathOk returns a tuple with the Path field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPath

`func (o *CodeBuildRequest) SetPath(v string)`

SetPath sets Path field to given value.


### GetProject

`func (o *CodeBuildRequest) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeBuildRequest) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeBuildRequest) SetProject(v string)`

SetProject sets Project field to given value.


### GetEmbeddingCommand

`func (o *CodeBuildRequest) GetEmbeddingCommand() string`

GetEmbeddingCommand returns the EmbeddingCommand field if non-nil, zero value otherwise.

### GetEmbeddingCommandOk

`func (o *CodeBuildRequest) GetEmbeddingCommandOk() (*string, bool)`

GetEmbeddingCommandOk returns a tuple with the EmbeddingCommand field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingCommand

`func (o *CodeBuildRequest) SetEmbeddingCommand(v string)`

SetEmbeddingCommand sets EmbeddingCommand field to given value.

### HasEmbeddingCommand

`func (o *CodeBuildRequest) HasEmbeddingCommand() bool`

HasEmbeddingCommand returns a boolean if a field has been set.

### GetForce

`func (o *CodeBuildRequest) GetForce() bool`

GetForce returns the Force field if non-nil, zero value otherwise.

### GetForceOk

`func (o *CodeBuildRequest) GetForceOk() (*bool, bool)`

GetForceOk returns a tuple with the Force field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetForce

`func (o *CodeBuildRequest) SetForce(v bool)`

SetForce sets Force field to given value.

### HasForce

`func (o *CodeBuildRequest) HasForce() bool`

HasForce returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


