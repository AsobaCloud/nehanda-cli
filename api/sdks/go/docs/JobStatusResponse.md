# JobStatusResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Id** | **int64** |  | 
**Kind** | **string** |  | 
**DocumentId** | **int64** |  | 
**Project** | **string** |  | 
**Status** | **string** |  | 
**Attempts** | **int32** |  | 
**LastError** | Pointer to **string** |  | [optional] 
**ClaimedBy** | Pointer to **string** |  | [optional] 
**ClaimedAt** | Pointer to **string** |  | [optional] 
**CreatedAt** | Pointer to **string** |  | [optional] 
**UpdatedAt** | Pointer to **string** |  | [optional] 

## Methods

### NewJobStatusResponse

`func NewJobStatusResponse(id int64, kind string, documentId int64, project string, status string, attempts int32, ) *JobStatusResponse`

NewJobStatusResponse instantiates a new JobStatusResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewJobStatusResponseWithDefaults

`func NewJobStatusResponseWithDefaults() *JobStatusResponse`

NewJobStatusResponseWithDefaults instantiates a new JobStatusResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetId

`func (o *JobStatusResponse) GetId() int64`

GetId returns the Id field if non-nil, zero value otherwise.

### GetIdOk

`func (o *JobStatusResponse) GetIdOk() (*int64, bool)`

GetIdOk returns a tuple with the Id field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetId

`func (o *JobStatusResponse) SetId(v int64)`

SetId sets Id field to given value.


### GetKind

`func (o *JobStatusResponse) GetKind() string`

GetKind returns the Kind field if non-nil, zero value otherwise.

### GetKindOk

`func (o *JobStatusResponse) GetKindOk() (*string, bool)`

GetKindOk returns a tuple with the Kind field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetKind

`func (o *JobStatusResponse) SetKind(v string)`

SetKind sets Kind field to given value.


### GetDocumentId

`func (o *JobStatusResponse) GetDocumentId() int64`

GetDocumentId returns the DocumentId field if non-nil, zero value otherwise.

### GetDocumentIdOk

`func (o *JobStatusResponse) GetDocumentIdOk() (*int64, bool)`

GetDocumentIdOk returns a tuple with the DocumentId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDocumentId

`func (o *JobStatusResponse) SetDocumentId(v int64)`

SetDocumentId sets DocumentId field to given value.


### GetProject

`func (o *JobStatusResponse) GetProject() string`

GetProject returns the Project field if non-nil, zero value otherwise.

### GetProjectOk

`func (o *JobStatusResponse) GetProjectOk() (*string, bool)`

GetProjectOk returns a tuple with the Project field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetProject

`func (o *JobStatusResponse) SetProject(v string)`

SetProject sets Project field to given value.


### GetStatus

`func (o *JobStatusResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *JobStatusResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *JobStatusResponse) SetStatus(v string)`

SetStatus sets Status field to given value.


### GetAttempts

`func (o *JobStatusResponse) GetAttempts() int32`

GetAttempts returns the Attempts field if non-nil, zero value otherwise.

### GetAttemptsOk

`func (o *JobStatusResponse) GetAttemptsOk() (*int32, bool)`

GetAttemptsOk returns a tuple with the Attempts field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetAttempts

`func (o *JobStatusResponse) SetAttempts(v int32)`

SetAttempts sets Attempts field to given value.


### GetLastError

`func (o *JobStatusResponse) GetLastError() string`

GetLastError returns the LastError field if non-nil, zero value otherwise.

### GetLastErrorOk

`func (o *JobStatusResponse) GetLastErrorOk() (*string, bool)`

GetLastErrorOk returns a tuple with the LastError field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetLastError

`func (o *JobStatusResponse) SetLastError(v string)`

SetLastError sets LastError field to given value.

### HasLastError

`func (o *JobStatusResponse) HasLastError() bool`

HasLastError returns a boolean if a field has been set.

### GetClaimedBy

`func (o *JobStatusResponse) GetClaimedBy() string`

GetClaimedBy returns the ClaimedBy field if non-nil, zero value otherwise.

### GetClaimedByOk

`func (o *JobStatusResponse) GetClaimedByOk() (*string, bool)`

GetClaimedByOk returns a tuple with the ClaimedBy field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetClaimedBy

`func (o *JobStatusResponse) SetClaimedBy(v string)`

SetClaimedBy sets ClaimedBy field to given value.

### HasClaimedBy

`func (o *JobStatusResponse) HasClaimedBy() bool`

HasClaimedBy returns a boolean if a field has been set.

### GetClaimedAt

`func (o *JobStatusResponse) GetClaimedAt() string`

GetClaimedAt returns the ClaimedAt field if non-nil, zero value otherwise.

### GetClaimedAtOk

`func (o *JobStatusResponse) GetClaimedAtOk() (*string, bool)`

GetClaimedAtOk returns a tuple with the ClaimedAt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetClaimedAt

`func (o *JobStatusResponse) SetClaimedAt(v string)`

SetClaimedAt sets ClaimedAt field to given value.

### HasClaimedAt

`func (o *JobStatusResponse) HasClaimedAt() bool`

HasClaimedAt returns a boolean if a field has been set.

### GetCreatedAt

`func (o *JobStatusResponse) GetCreatedAt() string`

GetCreatedAt returns the CreatedAt field if non-nil, zero value otherwise.

### GetCreatedAtOk

`func (o *JobStatusResponse) GetCreatedAtOk() (*string, bool)`

GetCreatedAtOk returns a tuple with the CreatedAt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCreatedAt

`func (o *JobStatusResponse) SetCreatedAt(v string)`

SetCreatedAt sets CreatedAt field to given value.

### HasCreatedAt

`func (o *JobStatusResponse) HasCreatedAt() bool`

HasCreatedAt returns a boolean if a field has been set.

### GetUpdatedAt

`func (o *JobStatusResponse) GetUpdatedAt() string`

GetUpdatedAt returns the UpdatedAt field if non-nil, zero value otherwise.

### GetUpdatedAtOk

`func (o *JobStatusResponse) GetUpdatedAtOk() (*string, bool)`

GetUpdatedAtOk returns a tuple with the UpdatedAt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetUpdatedAt

`func (o *JobStatusResponse) SetUpdatedAt(v string)`

SetUpdatedAt sets UpdatedAt field to given value.

### HasUpdatedAt

`func (o *JobStatusResponse) HasUpdatedAt() bool`

HasUpdatedAt returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


