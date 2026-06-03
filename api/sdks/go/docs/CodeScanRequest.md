# CodeScanRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Project** | **string** |  | 
**RootPath** | **string** |  | 
**Force** | Pointer to **bool** |  | [optional] [default to false]

## Methods

### NewCodeScanRequest

`func NewCodeScanRequest(project string, rootPath string, ) *CodeScanRequest`

NewCodeScanRequest instantiates a new CodeScanRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeScanRequestWithDefaults

`func NewCodeScanRequestWithDefaults() *CodeScanRequest`

NewCodeScanRequestWithDefaults instantiates a new CodeScanRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetProject

`func (o *CodeScanRequest) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeScanRequest) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeScanRequest) SetProject(v string)`

SetProject sets Project field to given value.


### GetRootPath

`func (o *CodeScanRequest) GetRootPath() string`

GetRootPath returns the RootPath field if non-nil, zero value otherwise.

### GetRootPathOk

`func (o *CodeScanRequest) GetRootPathOk() (*string, bool)`

GetRootPathOk returns a tuple with the RootPath field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRootPath

`func (o *CodeScanRequest) SetRootPath(v string)`

SetRootPath sets RootPath field to given value.


### GetForce

`func (o *CodeScanRequest) GetForce() bool`

GetForce returns the Force field if non-nil, zero value otherwise.

### GetForceOk

`func (o *CodeScanRequest) GetForceOk() (*bool, bool)`

GetForceOk returns a tuple with the Force field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetForce

`func (o *CodeScanRequest) SetForce(v bool)`

SetForce sets Force field to given value.

### HasForce

`func (o *CodeScanRequest) HasForce() bool`

HasForce returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


