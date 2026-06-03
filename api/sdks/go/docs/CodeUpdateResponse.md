# CodeUpdateResponse

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

### NewCodeUpdateResponse

`func NewCodeUpdateResponse() *CodeUpdateResponse`

NewCodeUpdateResponse instantiates a new CodeUpdateResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeUpdateResponseWithDefaults

`func NewCodeUpdateResponseWithDefaults() *CodeUpdateResponse`

NewCodeUpdateResponseWithDefaults instantiates a new CodeUpdateResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeUpdateResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeUpdateResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeUpdateResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeUpdateResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProject

`func (o *CodeUpdateResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeUpdateResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeUpdateResponse) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *CodeUpdateResponse) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetFilesScanned

`func (o *CodeUpdateResponse) GetFilesScanned() int32`

GetFilesScanned returns the FilesScanned field if non-nil, zero value otherwise.

### GetFilesScannedOk

`func (o *CodeUpdateResponse) GetFilesScannedOk() (*int32, bool)`

GetFilesScannedOk returns a tuple with the FilesScanned field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesScanned

`func (o *CodeUpdateResponse) SetFilesScanned(v int32)`

SetFilesScanned sets FilesScanned field to given value.

### HasFilesScanned

`func (o *CodeUpdateResponse) HasFilesScanned() bool`

HasFilesScanned returns a boolean if a field has been set.

### GetFilesIndexed

`func (o *CodeUpdateResponse) GetFilesIndexed() int32`

GetFilesIndexed returns the FilesIndexed field if non-nil, zero value otherwise.

### GetFilesIndexedOk

`func (o *CodeUpdateResponse) GetFilesIndexedOk() (*int32, bool)`

GetFilesIndexedOk returns a tuple with the FilesIndexed field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesIndexed

`func (o *CodeUpdateResponse) SetFilesIndexed(v int32)`

SetFilesIndexed sets FilesIndexed field to given value.

### HasFilesIndexed

`func (o *CodeUpdateResponse) HasFilesIndexed() bool`

HasFilesIndexed returns a boolean if a field has been set.

### GetFilesSkipped

`func (o *CodeUpdateResponse) GetFilesSkipped() int32`

GetFilesSkipped returns the FilesSkipped field if non-nil, zero value otherwise.

### GetFilesSkippedOk

`func (o *CodeUpdateResponse) GetFilesSkippedOk() (*int32, bool)`

GetFilesSkippedOk returns a tuple with the FilesSkipped field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesSkipped

`func (o *CodeUpdateResponse) SetFilesSkipped(v int32)`

SetFilesSkipped sets FilesSkipped field to given value.

### HasFilesSkipped

`func (o *CodeUpdateResponse) HasFilesSkipped() bool`

HasFilesSkipped returns a boolean if a field has been set.

### GetFilesRemoved

`func (o *CodeUpdateResponse) GetFilesRemoved() int32`

GetFilesRemoved returns the FilesRemoved field if non-nil, zero value otherwise.

### GetFilesRemovedOk

`func (o *CodeUpdateResponse) GetFilesRemovedOk() (*int32, bool)`

GetFilesRemovedOk returns a tuple with the FilesRemoved field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilesRemoved

`func (o *CodeUpdateResponse) SetFilesRemoved(v int32)`

SetFilesRemoved sets FilesRemoved field to given value.

### HasFilesRemoved

`func (o *CodeUpdateResponse) HasFilesRemoved() bool`

HasFilesRemoved returns a boolean if a field has been set.

### GetChunksAdded

`func (o *CodeUpdateResponse) GetChunksAdded() int32`

GetChunksAdded returns the ChunksAdded field if non-nil, zero value otherwise.

### GetChunksAddedOk

`func (o *CodeUpdateResponse) GetChunksAddedOk() (*int32, bool)`

GetChunksAddedOk returns a tuple with the ChunksAdded field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunksAdded

`func (o *CodeUpdateResponse) SetChunksAdded(v int32)`

SetChunksAdded sets ChunksAdded field to given value.

### HasChunksAdded

`func (o *CodeUpdateResponse) HasChunksAdded() bool`

HasChunksAdded returns a boolean if a field has been set.

### GetChunksRemoved

`func (o *CodeUpdateResponse) GetChunksRemoved() int32`

GetChunksRemoved returns the ChunksRemoved field if non-nil, zero value otherwise.

### GetChunksRemovedOk

`func (o *CodeUpdateResponse) GetChunksRemovedOk() (*int32, bool)`

GetChunksRemovedOk returns a tuple with the ChunksRemoved field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunksRemoved

`func (o *CodeUpdateResponse) SetChunksRemoved(v int32)`

SetChunksRemoved sets ChunksRemoved field to given value.

### HasChunksRemoved

`func (o *CodeUpdateResponse) HasChunksRemoved() bool`

HasChunksRemoved returns a boolean if a field has been set.

### GetEmbeddingsAdded

`func (o *CodeUpdateResponse) GetEmbeddingsAdded() int32`

GetEmbeddingsAdded returns the EmbeddingsAdded field if non-nil, zero value otherwise.

### GetEmbeddingsAddedOk

`func (o *CodeUpdateResponse) GetEmbeddingsAddedOk() (*int32, bool)`

GetEmbeddingsAddedOk returns a tuple with the EmbeddingsAdded field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingsAdded

`func (o *CodeUpdateResponse) SetEmbeddingsAdded(v int32)`

SetEmbeddingsAdded sets EmbeddingsAdded field to given value.

### HasEmbeddingsAdded

`func (o *CodeUpdateResponse) HasEmbeddingsAdded() bool`

HasEmbeddingsAdded returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


