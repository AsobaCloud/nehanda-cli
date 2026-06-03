# CodeStructureResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Definitions** | Pointer to [**[]CodeDefinition**](CodeDefinition.md) |  | [optional] 

## Methods

### NewCodeStructureResponse

`func NewCodeStructureResponse() *CodeStructureResponse`

NewCodeStructureResponse instantiates a new CodeStructureResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeStructureResponseWithDefaults

`func NewCodeStructureResponseWithDefaults() *CodeStructureResponse`

NewCodeStructureResponseWithDefaults instantiates a new CodeStructureResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeStructureResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeStructureResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeStructureResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *CodeStructureResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetDefinitions

`func (o *CodeStructureResponse) GetDefinitions() []CodeDefinition`

GetDefinitions returns the Definitions field if non-nil, zero value otherwise.

### GetDefinitionsOk

`func (o *CodeStructureResponse) GetDefinitionsOk() (*[]CodeDefinition, bool)`

GetDefinitionsOk returns a tuple with the Definitions field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDefinitions

`func (o *CodeStructureResponse) SetDefinitions(v []CodeDefinition)`

SetDefinitions sets Definitions field to given value.

### HasDefinitions

`func (o *CodeStructureResponse) HasDefinitions() bool`

HasDefinitions returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


