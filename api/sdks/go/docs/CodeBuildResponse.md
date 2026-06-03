# CodeBuildResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Project** | Pointer to **string** |  | [optional] 
**FilesScanned** | Pointer to **int32** |  | [optional] 
**FilesIndexed** | Pointer to **int32** |  | [optional] 
**FilesSkipped** | Pointer to **int32** |  | [optional] 
**FilesRemoved** | Pointer to **int32** |  | [optional] 
**ChunksAdded** | Pointer to **int32** |  | [optional] 
**ChunksRemoved** | Pointer to **int32** |  | [optional] 
**EmbeddingsAdded** | Pointer to **int32** |  | [optional] 

## Methods

### NewCodeBuildResponse

`func NewCodeBuildResponse() *CodeBuildResponse`

NewCodeBuildResponse instantiates a new CodeBuildResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeBuildResponseWithDefaults

`func NewCodeBuildResponseWithDefaults() *CodeBuildResponse`

NewCodeBuildResponseWithDefaults instantiates a new CodeBuildResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeBuildResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeBuildResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeBuildResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeBuildResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProject

`func (o *CodeBuildResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeBuildResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeBuildResponse) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *CodeBuildResponse) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetFilesScanned

`func (o *CodeBuildResponse) GetFilesScanned() int32`

GetFilesScanned returns the FilesScanned field if non-nil, zero value otherwise.

### GetFilesScannedOk

`func (o *CodeBuildResponse) GetFilesScannedOk() (*int32, bool)`

GetFilesScannedOk returns a tuple with the FilesScanned field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesScanned

`func (o *CodeBuildResponse) SetFilesScanned(v int32)`

SetFilesScanned sets FilesScanned field to given value.

### HasFilesScanned

`func (o *CodeBuildResponse) HasFilesScanned() bool`

HasFilesScanned returns a boolean if a field has been set.

### GetFilesIndexed

`func (o *CodeBuildResponse) GetFilesIndexed() int32`

GetFilesIndexed returns the FilesIndexed field if non-nil, zero value otherwise.

### GetFilesIndexedOk

`func (o *CodeBuildResponse) GetFilesIndexedOk() (*int32, bool)`

GetFilesIndexedOk returns a tuple with the FilesIndexed field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesIndexed

`func (o *CodeBuildResponse) SetFilesIndexed(v int32)`

SetFilesIndexed sets FilesIndexed field to given value.

### HasFilesIndexed

`func (o *CodeBuildResponse) HasFilesIndexed() bool`

HasFilesIndexed returns a boolean if a field has been set.

### GetFilesSkipped

`func (o *CodeBuildResponse) GetFilesSkipped() int32`

GetFilesSkipped returns the FilesSkipped field if non-nil, zero value otherwise.

### GetFilesSkippedOk

`func (o *CodeBuildResponse) GetFilesSkippedOk() (*int32, bool)`

GetFilesSkippedOk returns a tuple with the FilesSkipped field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesSkipped

`func (o *CodeBuildResponse) SetFilesSkipped(v int32)`

SetFilesSkipped sets FilesSkipped field to given value.

### HasFilesSkipped

`func (o *CodeBuildResponse) HasFilesSkipped() bool`

HasFilesSkipped returns a boolean if a field has been set.

### GetFilesRemoved

`func (o *CodeBuildResponse) GetFilesRemoved() int32`

GetFilesRemoved returns the FilesRemoved field if non-nil, zero value otherwise.

### GetFilesRemovedOk

`func (o *CodeBuildResponse) GetFilesRemovedOk() (*int32, bool)`

GetFilesRemovedOk returns a tuple with the FilesRemoved field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesRemoved

`func (o *CodeBuildResponse) SetFilesRemoved(v int32)`

SetFilesRemoved sets FilesRemoved field to given value.

### HasFilesRemoved

`func (o *CodeBuildResponse) HasFilesRemoved() bool`

HasFilesRemoved returns a boolean if a field has been set.

### GetChunksAdded

`func (o *CodeBuildResponse) GetChunksAdded() int32`

GetChunksAdded returns the ChunksAdded field if non-nil, zero value otherwise.

### GetChunksAddedOk

`func (o *CodeBuildResponse) GetChunksAddedOk() (*int32, bool)`

GetChunksAddedOk returns a tuple with the ChunksAdded field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunksAdded

`func (o *CodeBuildResponse) SetChunksAdded(v int32)`

SetChunksAdded sets ChunksAdded field to given value.

### HasChunksAdded

`func (o *CodeBuildResponse) HasChunksAdded() bool`

HasChunksAdded returns a boolean if a field has been set.

### GetChunksRemoved

`func (o *CodeBuildResponse) GetChunksRemoved() int32`

GetChunksRemoved returns the ChunksRemoved field if non-nil, zero value otherwise.

### GetChunksRemovedOk

`func (o *CodeBuildResponse) GetChunksRemovedOk() (*int32, bool)`

GetChunksRemovedOk returns a tuple with the ChunksRemoved field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunksRemoved

`func (o *CodeBuildResponse) SetChunksRemoved(v int32)`

SetChunksRemoved sets ChunksRemoved field to given value.

### HasChunksRemoved

`func (o *CodeBuildResponse) HasChunksRemoved() bool`

HasChunksRemoved returns a boolean if a field has been set.

### GetEmbeddingsAdded

`func (o *CodeBuildResponse) GetEmbeddingsAdded() int32`

GetEmbeddingsAdded returns the EmbeddingsAdded field if non-nil, zero value otherwise.

### GetEmbeddingsAddedOk

`func (o *CodeBuildResponse) GetEmbeddingsAddedOk() (*int32, bool)`

GetEmbeddingsAddedOk returns a tuple with the EmbeddingsAdded field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingsAdded

`func (o *CodeBuildResponse) SetEmbeddingsAdded(v int32)`

SetEmbeddingsAdded sets EmbeddingsAdded field to given value.

### HasEmbeddingsAdded

`func (o *CodeBuildResponse) HasEmbeddingsAdded() bool`

HasEmbeddingsAdded returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


