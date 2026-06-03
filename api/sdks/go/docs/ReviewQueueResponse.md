# ReviewQueueResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Docs** | Pointer to [**[]DocMetadataResponse**](DocMetadataResponse.md) |  | [optional] 
**NextCursor** | Pointer to **int64** |  | [optional] 

## Methods

### NewReviewQueueResponse

`func NewReviewQueueResponse() *ReviewQueueResponse`

NewReviewQueueResponse instantiates a new ReviewQueueResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewReviewQueueResponseWithDefaults

`func NewReviewQueueResponseWithDefaults() *ReviewQueueResponse`

NewReviewQueueResponseWithDefaults instantiates a new ReviewQueueResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetDocs

`func (o *ReviewQueueResponse) GetDocs() []DocMetadataResponse`

GetDocs returns the Docs field if non-nil, zero value otherwise.

### GetDocsOk

`func (o *ReviewQueueResponse) GetDocsOk() (*[]DocMetadataResponse, bool)`

GetDocsOk returns a tuple with the Docs field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDocs

`func (o *ReviewQueueResponse) SetDocs(v []DocMetadataResponse)`

SetDocs sets Docs field to given value.

### HasDocs

`func (o *ReviewQueueResponse) HasDocs() bool`

HasDocs returns a boolean if a field has been set.

### GetNextCursor

`func (o *ReviewQueueResponse) GetNextCursor() int64`

GetNextCursor returns the NextCursor field if non-nil, zero value otherwise.

### GetNextCursorOk

`func (o *ReviewQueueResponse) GetNextCursorOk() (*int64, bool)`

GetNextCursorOk returns a tuple with the NextCursor field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetNextCursor

`func (o *ReviewQueueResponse) SetNextCursor(v int64)`

SetNextCursor sets NextCursor field to given value.

### HasNextCursor

`func (o *ReviewQueueResponse) HasNextCursor() bool`

HasNextCursor returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


