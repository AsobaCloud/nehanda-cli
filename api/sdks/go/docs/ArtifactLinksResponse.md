# ArtifactLinksResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ArtifactId** | Pointer to **string** |  | [optional] 
**Links** | Pointer to [**[]ArtifactLinksResponseLinksInner**](ArtifactLinksResponseLinksInner.md) |  | [optional] 

## Methods

### NewArtifactLinksResponse

`func NewArtifactLinksResponse() *ArtifactLinksResponse`

NewArtifactLinksResponse instantiates a new ArtifactLinksResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewArtifactLinksResponseWithDefaults

`func NewArtifactLinksResponseWithDefaults() *ArtifactLinksResponse`

NewArtifactLinksResponseWithDefaults instantiates a new ArtifactLinksResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetArtifactId

`func (o *ArtifactLinksResponse) GetArtifactId() string`

GetArtifactId returns the ArtifactId field if non-nil, zero value otherwise.

### GetArtifactIdOk

`func (o *ArtifactLinksResponse) GetArtifactIdOk() (*string, bool)`

GetArtifactIdOk returns a tuple with the ArtifactId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetArtifactId

`func (o *ArtifactLinksResponse) SetArtifactId(v string)`

SetArtifactId sets ArtifactId field to given value.

### HasArtifactId

`func (o *ArtifactLinksResponse) HasArtifactId() bool`

HasArtifactId returns a boolean if a field has been set.

### GetLinks

`func (o *ArtifactLinksResponse) GetLinks() []ArtifactLinksResponseLinksInner`

GetLinks returns the Links field if non-nil, zero value otherwise.

### GetLinksOk

`func (o *ArtifactLinksResponse) GetLinksOk() (*[]ArtifactLinksResponseLinksInner, bool)`

GetLinksOk returns a tuple with the Links field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetLinks

`func (o *ArtifactLinksResponse) SetLinks(v []ArtifactLinksResponseLinksInner)`

SetLinks sets Links field to given value.

### HasLinks

`func (o *ArtifactLinksResponse) HasLinks() bool`

HasLinks returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


