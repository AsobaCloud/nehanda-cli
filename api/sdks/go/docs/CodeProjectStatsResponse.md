# CodeProjectStatsResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Project** | Pointer to **string** |  | [optional] 
**Files** | Pointer to **int32** |  | [optional] 
**Definitions** | Pointer to **int32** |  | [optional] 
**Langs** | Pointer to [**[]CodeProjectLanguage**](CodeProjectLanguage.md) |  | [optional] 

## Methods

### NewCodeProjectStatsResponse

`func NewCodeProjectStatsResponse() *CodeProjectStatsResponse`

NewCodeProjectStatsResponse instantiates a new CodeProjectStatsResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeProjectStatsResponseWithDefaults

`func NewCodeProjectStatsResponseWithDefaults() *CodeProjectStatsResponse`

NewCodeProjectStatsResponseWithDefaults instantiates a new CodeProjectStatsResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeProjectStatsResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeProjectStatsResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeProjectStatsResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeProjectStatsResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetProject

`func (o *CodeProjectStatsResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeProjectStatsResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeProjectStatsResponse) SetProject(v string)`

SetProject sets Project field to given value.

### HasProject

`func (o *CodeProjectStatsResponse) HasProject() bool`

HasProject returns a boolean if a field has been set.

### GetFiles

`func (o *CodeProjectStatsResponse) GetFiles() int32`

GetFiles returns the Files field if non-nil, zero value otherwise.

### GetFilesOk

`func (o *CodeProjectStatsResponse) GetFilesOk() (*int32, bool)`

GetFilesOk returns a tuple with the Files field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFiles

`func (o *CodeProjectStatsResponse) SetFiles(v int32)`

SetFiles sets Files field to given value.

### HasFiles

`func (o *CodeProjectStatsResponse) HasFiles() bool`

HasFiles returns a boolean if a field has been set.

### GetDefinitions

`func (o *CodeProjectStatsResponse) GetDefinitions() int32`

GetDefinitions returns the Definitions field if non-nil, zero value otherwise.

### GetDefinitionsOk

`func (o *CodeProjectStatsResponse) GetDefinitionsOk() (*int32, bool)`

GetDefinitionsOk returns a tuple with the Definitions field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDefinitions

`func (o *CodeProjectStatsResponse) SetDefinitions(v int32)`

SetDefinitions sets Definitions field to given value.

### HasDefinitions

`func (o *CodeProjectStatsResponse) HasDefinitions() bool`

HasDefinitions returns a boolean if a field has been set.

### GetLangs

`func (o *CodeProjectStatsResponse) GetLangs() []CodeProjectLanguage`

GetLangs returns the Langs field if non-nil, zero value otherwise.

### GetLangsOk

`func (o *CodeProjectStatsResponse) GetLangsOk() (*[]CodeProjectLanguage, bool)`

GetLangsOk returns a tuple with the Langs field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetLangs

`func (o *CodeProjectStatsResponse) SetLangs(v []CodeProjectLanguage)`

SetLangs sets Langs field to given value.

### HasLangs

`func (o *CodeProjectStatsResponse) HasLangs() bool`

HasLangs returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


