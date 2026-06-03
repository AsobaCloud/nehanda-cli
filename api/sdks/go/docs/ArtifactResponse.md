# ArtifactResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Id** | Pointer to **string** |  | [optional] 
**Kind** | Pointer to **string** |  | [optional] 
**State** | Pointer to **string** |  | [optional] 
**ScopeKind** | Pointer to **string** |  | [optional] 
**ScopeId** | Pointer to **string** |  | [optional] 
**Confidence** | Pointer to **float64** |  | [optional] 
**Payload** | Pointer to **map[string]interface{}** |  | [optional] 
**Citations** | Pointer to [**[]SearchHitCitationsInner**](SearchHitCitationsInner.md) |  | [optional] 
**UpdatedAt** | Pointer to **time.Time** |  | [optional] 

## Methods

### NewArtifactResponse

`func NewArtifactResponse() *ArtifactResponse`

NewArtifactResponse instantiates a new ArtifactResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewArtifactResponseWithDefaults

`func NewArtifactResponseWithDefaults() *ArtifactResponse`

NewArtifactResponseWithDefaults instantiates a new ArtifactResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetId

`func (o *ArtifactResponse) GetId() string`

GetId returns the Id field if non-nil, zero value otherwise.

### GetIdOk

`func (o *ArtifactResponse) GetIdOk() (*string, bool)`

GetIdOk returns a tuple with the Id field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetId

`func (o *ArtifactResponse) SetId(v string)`

SetId sets Id field to given value.

### HasId

`func (o *ArtifactResponse) HasId() bool`

HasId returns a boolean if a field has been set.

### GetKind

`func (o *ArtifactResponse) GetKind() string`

GetKind returns the Kind field if non-nil, zero value otherwise.

### GetKindOk

`func (o *ArtifactResponse) GetKindOk() (*string, bool)`

GetKindOk returns a tuple with the Kind field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetKind

`func (o *ArtifactResponse) SetKind(v string)`

SetKind sets Kind field to given value.

### HasKind

`func (o *ArtifactResponse) HasKind() bool`

HasKind returns a boolean if a field has been set.

### GetState

`func (o *ArtifactResponse) GetState() string`

GetState returns the State field if non-nil, zero value otherwise.

### GetStateOk

`func (o *ArtifactResponse) GetStateOk() (*string, bool)`

GetStateOk returns a tuple with the State field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetState

`func (o *ArtifactResponse) SetState(v string)`

SetState sets State field to given value.

### HasState

`func (o *ArtifactResponse) HasState() bool`

HasState returns a boolean if a field has been set.

### GetScopeKind

`func (o *ArtifactResponse) GetScopeKind() string`

GetScopeKind returns the ScopeKind field if non-nil, zero value otherwise.

### GetScopeKindOk

`func (o *ArtifactResponse) GetScopeKindOk() (*string, bool)`

GetScopeKindOk returns a tuple with the ScopeKind field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScopeKind

`func (o *ArtifactResponse) SetScopeKind(v string)`

SetScopeKind sets ScopeKind field to given value.

### HasScopeKind

`func (o *ArtifactResponse) HasScopeKind() bool`

HasScopeKind returns a boolean if a field has been set.

### GetScopeId

`func (o *ArtifactResponse) GetScopeId() string`

GetScopeId returns the ScopeId field if non-nil, zero value otherwise.

### GetScopeIdOk

`func (o *ArtifactResponse) GetScopeIdOk() (*string, bool)`

GetScopeIdOk returns a tuple with the ScopeId field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScopeId

`func (o *ArtifactResponse) SetScopeId(v string)`

SetScopeId sets ScopeId field to given value.

### HasScopeId

`func (o *ArtifactResponse) HasScopeId() bool`

HasScopeId returns a boolean if a field has been set.

### GetConfidence

`func (o *ArtifactResponse) GetConfidence() float64`

GetConfidence returns the Confidence field if non-nil, zero value otherwise.

### GetConfidenceOk

`func (o *ArtifactResponse) GetConfidenceOk() (*float64, bool)`

GetConfidenceOk returns a tuple with the Confidence field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetConfidence

`func (o *ArtifactResponse) SetConfidence(v float64)`

SetConfidence sets Confidence field to given value.

### HasConfidence

`func (o *ArtifactResponse) HasConfidence() bool`

HasConfidence returns a boolean if a field has been set.

### GetPayload

`func (o *ArtifactResponse) GetPayload() map[string]interface{}`

GetPayload returns the Payload field if non-nil, zero value otherwise.

### GetPayloadOk

`func (o *ArtifactResponse) GetPayloadOk() (*map[string]interface{}, bool)`

GetPayloadOk returns a tuple with the Payload field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPayload

`func (o *ArtifactResponse) SetPayload(v map[string]interface{})`

SetPayload sets Payload field to given value.

### HasPayload

`func (o *ArtifactResponse) HasPayload() bool`

HasPayload returns a boolean if a field has been set.

### GetCitations

`func (o *ArtifactResponse) GetCitations() []SearchHitCitationsInner`

GetCitations returns the Citations field if non-nil, zero value otherwise.

### GetCitationsOk

`func (o *ArtifactResponse) GetCitationsOk() (*[]SearchHitCitationsInner, bool)`

GetCitationsOk returns a tuple with the Citations field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetCitations

`func (o *ArtifactResponse) SetCitations(v []SearchHitCitationsInner)`

SetCitations sets Citations field to given value.

### HasCitations

`func (o *ArtifactResponse) HasCitations() bool`

HasCitations returns a boolean if a field has been set.

### GetUpdatedAt

`func (o *ArtifactResponse) GetUpdatedAt() time.Time`

GetUpdatedAt returns the UpdatedAt field if non-nil, zero value otherwise.

### GetUpdatedAtOk

`func (o *ArtifactResponse) GetUpdatedAtOk() (*time.Time, bool)`

GetUpdatedAtOk returns a tuple with the UpdatedAt field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetUpdatedAt

`func (o *ArtifactResponse) SetUpdatedAt(v time.Time)`

SetUpdatedAt sets UpdatedAt field to given value.

### HasUpdatedAt

`func (o *ArtifactResponse) HasUpdatedAt() bool`

HasUpdatedAt returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


