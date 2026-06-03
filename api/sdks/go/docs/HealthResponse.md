# HealthResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | **string** | ok when healthy; a non-ok string when degraded | 
**Db2Ok** | Pointer to **bool** | DB2 (Postgres) reachable | [optional] 
**Db2KbTablesOk** | Pointer to **bool** | required kb tables present | [optional] 
**PgvecOk** | Pointer to **bool** | pgvector extension available | [optional] 
**PgvecCollectionOk** | Pointer to **bool** |  | [optional] 
**EmbedOk** | Pointer to **bool** |  | [optional] 
**EmbedCommand** | Pointer to **string** |  | [optional] 
**ChunkCount** | Pointer to **int32** |  | [optional] 
**EmbeddingCount** | Pointer to **int32** |  | [optional] 
**Warnings** | Pointer to **[]string** |  | [optional] 

## Methods

### NewHealthResponse

`func NewHealthResponse(status string, ) *HealthResponse`

NewHealthResponse instantiates a new HealthResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewHealthResponseWithDefaults

`func NewHealthResponseWithDefaults() *HealthResponse`

NewHealthResponseWithDefaults instantiates a new HealthResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *HealthResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *HealthResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *HealthResponse) SetStatus(v string)`

SetStatus sets Status field to given value.


### GetDb2Ok

`func (o *HealthResponse) GetDb2Ok() bool`

GetDb2Ok returns the Db2Ok field if non-nil, zero value otherwise.

### GetDb2OkOk

`func (o *HealthResponse) GetDb2OkOk() (*bool, bool)`

GetDb2OkOk returns a tuple with the Db2Ok field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDb2Ok

`func (o *HealthResponse) SetDb2Ok(v bool)`

SetDb2Ok sets Db2Ok field to given value.

### HasDb2Ok

`func (o *HealthResponse) HasDb2Ok() bool`

HasDb2Ok returns a boolean if a field has been set.

### GetDb2KbTablesOk

`func (o *HealthResponse) GetDb2KbTablesOk() bool`

GetDb2KbTablesOk returns the Db2KbTablesOk field if non-nil, zero value otherwise.

### GetDb2KbTablesOkOk

`func (o *HealthResponse) GetDb2KbTablesOkOk() (*bool, bool)`

GetDb2KbTablesOkOk returns a tuple with the Db2KbTablesOk field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDb2KbTablesOk

`func (o *HealthResponse) SetDb2KbTablesOk(v bool)`

SetDb2KbTablesOk sets Db2KbTablesOk field to given value.

### HasDb2KbTablesOk

`func (o *HealthResponse) HasDb2KbTablesOk() bool`

HasDb2KbTablesOk returns a boolean if a field has been set.

### GetPgvecOk

`func (o *HealthResponse) GetPgvecOk() bool`

GetPgvecOk returns the PgvecOk field if non-nil, zero value otherwise.

### GetPgvecOkOk

`func (o *HealthResponse) GetPgvecOkOk() (*bool, bool)`

GetPgvecOkOk returns a tuple with the PgvecOk field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPgvecOk

`func (o *HealthResponse) SetPgvecOk(v bool)`

SetPgvecOk sets PgvecOk field to given value.

### HasPgvecOk

`func (o *HealthResponse) HasPgvecOk() bool`

HasPgvecOk returns a boolean if a field has been set.

### GetPgvecCollectionOk

`func (o *HealthResponse) GetPgvecCollectionOk() bool`

GetPgvecCollectionOk returns the PgvecCollectionOk field if non-nil, zero value otherwise.

### GetPgvecCollectionOkOk

`func (o *HealthResponse) GetPgvecCollectionOkOk() (*bool, bool)`

GetPgvecCollectionOkOk returns a tuple with the PgvecCollectionOk field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetPgvecCollectionOk

`func (o *HealthResponse) SetPgvecCollectionOk(v bool)`

SetPgvecCollectionOk sets PgvecCollectionOk field to given value.

### HasPgvecCollectionOk

`func (o *HealthResponse) HasPgvecCollectionOk() bool`

HasPgvecCollectionOk returns a boolean if a field has been set.

### GetEmbedOk

`func (o *HealthResponse) GetEmbedOk() bool`

GetEmbedOk returns the EmbedOk field if non-nil, zero value otherwise.

### GetEmbedOkOk

`func (o *HealthResponse) GetEmbedOkOk() (*bool, bool)`

GetEmbedOkOk returns a tuple with the EmbedOk field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbedOk

`func (o *HealthResponse) SetEmbedOk(v bool)`

SetEmbedOk sets EmbedOk field to given value.

### HasEmbedOk

`func (o *HealthResponse) HasEmbedOk() bool`

HasEmbedOk returns a boolean if a field has been set.

### GetEmbedCommand

`func (o *HealthResponse) GetEmbedCommand() string`

GetEmbedCommand returns the EmbedCommand field if non-nil, zero value otherwise.

### GetEmbedCommandOk

`func (o *HealthResponse) GetEmbedCommandOk() (*string, bool)`

GetEmbedCommandOk returns a tuple with the EmbedCommand field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbedCommand

`func (o *HealthResponse) SetEmbedCommand(v string)`

SetEmbedCommand sets EmbedCommand field to given value.

### HasEmbedCommand

`func (o *HealthResponse) HasEmbedCommand() bool`

HasEmbedCommand returns a boolean if a field has been set.

### GetChunkCount

`func (o *HealthResponse) GetChunkCount() int32`

GetChunkCount returns the ChunkCount field if non-nil, zero value otherwise.

### GetChunkCountOk

`func (o *HealthResponse) GetChunkCountOk() (*int32, bool)`

GetChunkCountOk returns a tuple with the ChunkCount field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetChunkCount

`func (o *HealthResponse) SetChunkCount(v int32)`

SetChunkCount sets ChunkCount field to given value.

### HasChunkCount

`func (o *HealthResponse) HasChunkCount() bool`

HasChunkCount returns a boolean if a field has been set.

### GetEmbeddingCount

`func (o *HealthResponse) GetEmbeddingCount() int32`

GetEmbeddingCount returns the EmbeddingCount field if non-nil, zero value otherwise.

### GetEmbeddingCountOk

`func (o *HealthResponse) GetEmbeddingCountOk() (*int32, bool)`

GetEmbeddingCountOk returns a tuple with the EmbeddingCount field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetEmbeddingCount

`func (o *HealthResponse) SetEmbeddingCount(v int32)`

SetEmbeddingCount sets EmbeddingCount field to given value.

### HasEmbeddingCount

`func (o *HealthResponse) HasEmbeddingCount() bool`

HasEmbeddingCount returns a boolean if a field has been set.

### GetWarnings

`func (o *HealthResponse) GetWarnings() []string`

GetWarnings returns the Warnings field if non-nil, zero value otherwise.

### GetWarningsOk

`func (o *HealthResponse) GetWarningsOk() (*[]string, bool)`

GetWarningsOk returns a tuple with the Warnings field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetWarnings

`func (o *HealthResponse) SetWarnings(v []string)`

SetWarnings sets Warnings field to given value.

### HasWarnings

`func (o *HealthResponse) HasWarnings() bool`

HasWarnings returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


