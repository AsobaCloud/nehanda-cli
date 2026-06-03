# RollbackRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**TargetReleaseId** | Pointer to **int64** |  | [optional] 

## Methods

### NewRollbackRequest

`func NewRollbackRequest() *RollbackRequest`

NewRollbackRequest instantiates a new RollbackRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewRollbackRequestWithDefaults

`func NewRollbackRequestWithDefaults() *RollbackRequest`

NewRollbackRequestWithDefaults instantiates a new RollbackRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetTargetReleaseId

`func (o *RollbackRequest) GetTargetReleaseId() int64`

GetTargetReleaseId returns the TargetReleaseId field if non-nil, zero value otherwise.

### GetTargetReleaseIdOk

`func (o *RollbackRequest) GetTargetReleaseIdOk() (*int64, bool)`

GetTargetReleaseIdOk returns a tuple with the TargetReleaseId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetTargetReleaseId

`func (o *RollbackRequest) SetTargetReleaseId(v int64)`

SetTargetReleaseId sets TargetReleaseId field to given value.

### HasTargetReleaseId

`func (o *RollbackRequest) HasTargetReleaseId() bool`

HasTargetReleaseId returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


