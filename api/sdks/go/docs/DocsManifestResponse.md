# DocsManifestResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Missing** | [**[]DocsManifestResponseMissingInner**](DocsManifestResponseMissingInner.md) |  | 
**Total** | **int32** |  | 
**Present** | **int32** |  | 
**MissingCount** | **int32** |  | 

## Methods

### NewDocsManifestResponse

`func NewDocsManifestResponse(missing []DocsManifestResponseMissingInner, total int32, present int32, missingCount int32, ) *DocsManifestResponse`

NewDocsManifestResponse instantiates a new DocsManifestResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewDocsManifestResponseWithDefaults

`func NewDocsManifestResponseWithDefaults() *DocsManifestResponse`

NewDocsManifestResponseWithDefaults instantiates a new DocsManifestResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetMissing

`func (o *DocsManifestResponse) GetMissing() []DocsManifestResponseMissingInner`

GetMissing returns the Missing field if non-nil, zero value otherwise.

### GetMissingOk

`func (o *DocsManifestResponse) GetMissingOk() (*[]DocsManifestResponseMissingInner, bool)`

GetMissingOk returns a tuple with the Missing field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetMissing

`func (o *DocsManifestResponse) SetMissing(v []DocsManifestResponseMissingInner)`

SetMissing sets Missing field to given value.


### GetTotal

`func (o *DocsManifestResponse) GetTotal() int32`

GetTotal returns the Total field if non-nil, zero value otherwise.

### GetTotalOk

`func (o *DocsManifestResponse) GetTotalOk() (*int32, bool)`

GetTotalOk returns a tuple with the Total field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetTotal

`func (o *DocsManifestResponse) SetTotal(v int32)`

SetTotal sets Total field to given value.


### GetPresent

`func (o *DocsManifestResponse) GetPresent() int32`

GetPresent returns the Present field if non-nil, zero value otherwise.

### GetPresentOk

`func (o *DocsManifestResponse) GetPresentOk() (*int32, bool)`

GetPresentOk returns a tuple with the Present field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPresent

`func (o *DocsManifestResponse) SetPresent(v int32)`

SetPresent sets Present field to given value.


### GetMissingCount

`func (o *DocsManifestResponse) GetMissingCount() int32`

GetMissingCount returns the MissingCount field if non-nil, zero value otherwise.

### GetMissingCountOk

`func (o *DocsManifestResponse) GetMissingCountOk() (*int32, bool)`

GetMissingCountOk returns a tuple with the MissingCount field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetMissingCount

`func (o *DocsManifestResponse) SetMissingCount(v int32)`

SetMissingCount sets MissingCount field to given value.



[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


