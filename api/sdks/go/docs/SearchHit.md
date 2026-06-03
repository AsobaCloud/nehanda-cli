# SearchHit

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ArtifactId** | Pointer to **string** |  | [optional] 
**Score** | Pointer to **float64** |  | [optional] 
**Kind** | Pointer to **string** |  | [optional] 
**Excerpt** | Pointer to **string** |  | [optional] 
**Citations** | Pointer to [**[]SearchHitCitationsInner**](SearchHitCitationsInner.md) |  | [optional] 

## Methods

### NewSearchHit

`func NewSearchHit() *SearchHit`

NewSearchHit instantiates a new SearchHit object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewSearchHitWithDefaults

`func NewSearchHitWithDefaults() *SearchHit`

NewSearchHitWithDefaults instantiates a new SearchHit object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetArtifactId

`func (o *SearchHit) GetArtifactId() string`

GetArtifactId returns the ArtifactId field if non-nil, zero value otherwise.

### GetArtifactIdOk

`func (o *SearchHit) GetArtifactIdOk() (*string, bool)`

GetArtifactIdOk returns a tuple with the ArtifactId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetArtifactId

`func (o *SearchHit) SetArtifactId(v string)`

SetArtifactId sets ArtifactId field to given value.

### HasArtifactId

`func (o *SearchHit) HasArtifactId() bool`

HasArtifactId returns a boolean if a field has been set.

### GetScore

`func (o *SearchHit) GetScore() float64`

GetScore returns the Score field if non-nil, zero value otherwise.

### GetScoreOk

`func (o *SearchHit) GetScoreOk() (*float64, bool)`

GetScoreOk returns a tuple with the Score field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScore

`func (o *SearchHit) SetScore(v float64)`

SetScore sets Score field to given value.

### HasScore

`func (o *SearchHit) HasScore() bool`

HasScore returns a boolean if a field has been set.

### GetKind

`func (o *SearchHit) GetKind() string`

GetKind returns the Kind field if non-nil, zero value otherwise.

### GetKindOk

`func (o *SearchHit) GetKindOk() (*string, bool)`

GetKindOk returns a tuple with the Kind field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetKind

`func (o *SearchHit) SetKind(v string)`

SetKind sets Kind field to given value.

### HasKind

`func (o *SearchHit) HasKind() bool`

HasKind returns a boolean if a field has been set.

### GetExcerpt

`func (o *SearchHit) GetExcerpt() string`

GetExcerpt returns the Excerpt field if non-nil, zero value otherwise.

### GetExcerptOk

`func (o *SearchHit) GetExcerptOk() (*string, bool)`

GetExcerptOk returns a tuple with the Excerpt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetExcerpt

`func (o *SearchHit) SetExcerpt(v string)`

SetExcerpt sets Excerpt field to given value.

### HasExcerpt

`func (o *SearchHit) HasExcerpt() bool`

HasExcerpt returns a boolean if a field has been set.

### GetCitations

`func (o *SearchHit) GetCitations() []SearchHitCitationsInner`

GetCitations returns the Citations field if non-nil, zero value otherwise.

### GetCitationsOk

`func (o *SearchHit) GetCitationsOk() (*[]SearchHitCitationsInner, bool)`

GetCitationsOk returns a tuple with the Citations field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCitations

`func (o *SearchHit) SetCitations(v []SearchHitCitationsInner)`

SetCitations sets Citations field to given value.

### HasCitations

`func (o *SearchHit) HasCitations() bool`

HasCitations returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


