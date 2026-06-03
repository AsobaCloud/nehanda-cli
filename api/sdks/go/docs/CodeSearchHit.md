# CodeSearchHit

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Project** | Pointer to **string** |  | [optional] 
**FilePath** | Pointer to **string** |  | [optional] 
**Snippet** | Pointer to **string** |  | [optional] 
**Rank** | Pointer to **float64** |  | [optional] 

## Methods

### NewCodeSearchHit

`func NewCodeSearchHit() *CodeSearchHit`

NewCodeSearchHit instantiates a new CodeSearchHit object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeSearchHitWithDefaults

`func NewCodeSearchHitWithDefaults() *CodeSearchHit`

NewCodeSearchHitWithDefaults instantiates a new CodeSearchHit object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetProject

`func (o *CodeSearchHit) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeSearchHit) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeSearchHit) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *CodeSearchHit) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetFilePath

`func (o *CodeSearchHit) GetFilePath() string`

GetFilePath returns the FilePath field if non-nil, zero value otherwise.

### GetFilePathOk

`func (o *CodeSearchHit) GetFilePathOk() (*string, bool)`

GetFilePathOk returns a tuple with the FilePath field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFilePath

`func (o *CodeSearchHit) SetFilePath(v string)`

SetFilePath sets FilePath field to given value.

### HasFilePath

`func (o *CodeSearchHit) HasFilePath() bool`

HasFilePath returns a boolean if a field has been set.

### GetSnippet

`func (o *CodeSearchHit) GetSnippet() string`

GetSnippet returns the Snippet field if non-nil, zero value otherwise.

### GetSnippetOk

`func (o *CodeSearchHit) GetSnippetOk() (*string, bool)`

GetSnippetOk returns a tuple with the Snippet field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetSnippet

`func (o *CodeSearchHit) SetSnippet(v string)`

SetSnippet sets Snippet field to given value.

### HasSnippet

`func (o *CodeSearchHit) HasSnippet() bool`

HasSnippet returns a boolean if a field has been set.

### GetRank

`func (o *CodeSearchHit) GetRank() float64`

GetRank returns the Rank field if non-nil, zero value otherwise.

### GetRankOk

`func (o *CodeSearchHit) GetRankOk() (*float64, bool)`

GetRankOk returns a tuple with the Rank field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRank

`func (o *CodeSearchHit) SetRank(v float64)`

SetRank sets Rank field to given value.

### HasRank

`func (o *CodeSearchHit) HasRank() bool`

HasRank returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


