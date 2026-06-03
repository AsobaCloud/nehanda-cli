# IngestRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Workspace** | Pointer to **string** | Workspace root, workspace name, or \&quot;all\&quot;. | [optional] 
**EmbeddingCommand** | Pointer to **string** |  | [optional] 
**Force** | Pointer to **bool** |  | [optional] [default to false]

## Methods

### NewIngestRequest

`func NewIngestRequest() *IngestRequest`

NewIngestRequest instantiates a new IngestRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewIngestRequestWithDefaults

`func NewIngestRequestWithDefaults() *IngestRequest`

NewIngestRequestWithDefaults instantiates a new IngestRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetWorkspace

`func (o *IngestRequest) GetWorkspace() string`

GetWorkspace returns the Workspace field if non-nil, zero value otherwise.

### GetWorkspaceOk

`func (o *IngestRequest) GetWorkspaceOk() (*string, bool)`

GetWorkspaceOk returns a tuple with the Workspace field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetWorkspace

`func (o *IngestRequest) SetWorkspace(v string)`

SetWorkspace sets Workspace field to given value.

### HasWorkspace

`func (o *IngestRequest) HasWorkspace() bool`

HasWorkspace returns a boolean if a field has been set.

### GetEmbeddingCommand

`func (o *IngestRequest) GetEmbeddingCommand() string`

GetEmbeddingCommand returns the EmbeddingCommand field if non-nil, zero value otherwise.

### GetEmbeddingCommandOk

`func (o *IngestRequest) GetEmbeddingCommandOk() (*string, bool)`

GetEmbeddingCommandOk returns a tuple with the EmbeddingCommand field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingCommand

`func (o *IngestRequest) SetEmbeddingCommand(v string)`

SetEmbeddingCommand sets EmbeddingCommand field to given value.

### HasEmbeddingCommand

`func (o *IngestRequest) HasEmbeddingCommand() bool`

HasEmbeddingCommand returns a boolean if a field has been set.

### GetForce

`func (o *IngestRequest) GetForce() bool`

GetForce returns the Force field if non-nil, zero value otherwise.

### GetForceOk

`func (o *IngestRequest) GetForceOk() (*bool, bool)`

GetForceOk returns a tuple with the Force field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetForce

`func (o *IngestRequest) SetForce(v bool)`

SetForce sets Force field to given value.

### HasForce

`func (o *IngestRequest) HasForce() bool`

HasForce returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


