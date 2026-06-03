# DocsManifestRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Scope** | Pointer to **string** |  | [optional] [default to "global"]
**Docs** | [**[]DocsManifestRequestDocsInner**](DocsManifestRequestDocsInner.md) |  | 

## Methods

### NewDocsManifestRequest

`func NewDocsManifestRequest(docs []DocsManifestRequestDocsInner, ) *DocsManifestRequest`

NewDocsManifestRequest instantiates a new DocsManifestRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewDocsManifestRequestWithDefaults

`func NewDocsManifestRequestWithDefaults() *DocsManifestRequest`

NewDocsManifestRequestWithDefaults instantiates a new DocsManifestRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetScope

`func (o *DocsManifestRequest) GetScope() string`

GetScope returns the Scope field if non-nil, zero value otherwise.

### GetScopeOk

`func (o *DocsManifestRequest) GetScopeOk() (*string, bool)`

GetScopeOk returns a tuple with the Scope field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetScope

`func (o *DocsManifestRequest) SetScope(v string)`

SetScope sets Scope field to given value.

### HasScope

`func (o *DocsManifestRequest) HasScope() bool`

HasScope returns a boolean if a field has been set.

### GetDocs

`func (o *DocsManifestRequest) GetDocs() []DocsManifestRequestDocsInner`

GetDocs returns the Docs field if non-nil, zero value otherwise.

### GetDocsOk

`func (o *DocsManifestRequest) GetDocsOk() (*[]DocsManifestRequestDocsInner, bool)`

GetDocsOk returns a tuple with the Docs field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDocs

`func (o *DocsManifestRequest) SetDocs(v []DocsManifestRequestDocsInner)`

SetDocs sets Docs field to given value.



[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


