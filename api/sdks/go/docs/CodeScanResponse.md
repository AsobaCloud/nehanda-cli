# CodeScanResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | **string** |  | 
**Skipped** | **bool** |  | 
**Project** | **string** |  | 
**Files** | **int32** |  | 
**Inspected** | **int32** |  | 

## Methods

### NewCodeScanResponse

`func NewCodeScanResponse(status string, skipped bool, project string, files int32, inspected int32, ) *CodeScanResponse`

NewCodeScanResponse instantiates a new CodeScanResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewCodeScanResponseWithDefaults

`func NewCodeScanResponseWithDefaults() *CodeScanResponse`

NewCodeScanResponseWithDefaults instantiates a new CodeScanResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *CodeScanResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *CodeScanResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *CodeScanResponse) SetStatus(v string)`

SetStatus sets Status field to given value.


### GetSkipped

`func (o *CodeScanResponse) GetSkipped() bool`

GetSkipped returns the Skipped field if non-nil, zero value otherwise.

### GetSkippedOk

`func (o *CodeScanResponse) GetSkippedOk() (*bool, bool)`

GetSkippedOk returns a tuple with the Skipped field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetSkipped

`func (o *CodeScanResponse) SetSkipped(v bool)`

SetSkipped sets Skipped field to given value.


### GetProject

`func (o *CodeScanResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *CodeScanResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *CodeScanResponse) SetProject(v string)`

SetProject sets Project field to given value.


### GetFiles

`func (o *CodeScanResponse) GetFiles() int32`

GetFiles returns the Files field if non-nil, zero value otherwise.

### GetFilesOk

`func (o *CodeScanResponse) GetFilesOk() (*int32, bool)`

GetFilesOk returns a tuple with the Files field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetFiles

`func (o *CodeScanResponse) SetFiles(v int32)`

SetFiles sets Files field to given value.


### GetInspected

`func (o *CodeScanResponse) GetInspected() int32`

GetInspected returns the Inspected field if non-nil, zero value otherwise.

### GetInspectedOk

`func (o *CodeScanResponse) GetInspectedOk() (*int32, bool)`

GetInspectedOk returns a tuple with the Inspected field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetInspected

`func (o *CodeScanResponse) SetInspected(v int32)`

SetInspected sets Inspected field to given value.



[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


