# MaintenanceReconcileResponse

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**Status** | Pointer to **string** |  | [optional] 
**Rc** | Pointer to **int32** |  | [optional] 
**DryRun** | Pointer to **bool** |  | [optional] 
**Memory** | Pointer to [**MaintenanceReconcileResponseMemory**](MaintenanceReconcileResponseMemory.md) |  | [optional] 
**Kb** | Pointer to [**MaintenanceReconcileResponseMemory**](MaintenanceReconcileResponseMemory.md) |  | [optional] 

## Methods

### NewMaintenanceReconcileResponse

`func NewMaintenanceReconcileResponse() *MaintenanceReconcileResponse`

NewMaintenanceReconcileResponse instantiates a new MaintenanceReconcileResponse object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewMaintenanceReconcileResponseWithDefaults

`func NewMaintenanceReconcileResponseWithDefaults() *MaintenanceReconcileResponse`

NewMaintenanceReconcileResponseWithDefaults instantiates a new MaintenanceReconcileResponse object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetStatus

`func (o *MaintenanceReconcileResponse) GetStatus() string`

GetStatus returns the Status field if non-nil, zero value otherwise.

### GetStatusOk

`func (o *MaintenanceReconcileResponse) GetStatusOk() (*string, bool)`

GetStatusOk returns a tuple with the Status field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetStatus

`func (o *MaintenanceReconcileResponse) SetStatus(v string)`

SetStatus sets Status field to given value.

### HasStatus

`func (o *MaintenanceReconcileResponse) HasStatus() bool`

HasStatus returns a boolean if a field has been set.

### GetRc

`func (o *MaintenanceReconcileResponse) GetRc() int32`

GetRc returns the Rc field if non-nil, zero value otherwise.

### GetRcOk

`func (o *MaintenanceReconcileResponse) GetRcOk() (*int32, bool)`

GetRcOk returns a tuple with the Rc field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetRc

`func (o *MaintenanceReconcileResponse) SetRc(v int32)`

SetRc sets Rc field to given value.

### HasRc

`func (o *MaintenanceReconcileResponse) HasRc() bool`

HasRc returns a boolean if a field has been set.

### GetDryRun

`func (o *MaintenanceReconcileResponse) GetDryRun() bool`

GetDryRun returns the DryRun field if non-nil, zero value otherwise.

### GetDryRunOk

`func (o *MaintenanceReconcileResponse) GetDryRunOk() (*bool, bool)`

GetDryRunOk returns a tuple with the DryRun field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDryRun

`func (o *MaintenanceReconcileResponse) SetDryRun(v bool)`

SetDryRun sets DryRun field to given value.

### HasDryRun

`func (o *MaintenanceReconcileResponse) HasDryRun() bool`

HasDryRun returns a boolean if a field has been set.

### GetMemory

`func (o *MaintenanceReconcileResponse) GetMemory() MaintenanceReconcileResponseMemory`

GetMemory returns the Memory field if non-nil, zero value otherwise.

### GetMemoryOk

`func (o *MaintenanceReconcileResponse) GetMemoryOk() (*MaintenanceReconcileResponseMemory, bool)`

GetMemoryOk returns a tuple with the Memory field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetMemory

`func (o *MaintenanceReconcileResponse) SetMemory(v MaintenanceReconcileResponseMemory)`

SetMemory sets Memory field to given value.

### HasMemory

`func (o *MaintenanceReconcileResponse) HasMemory() bool`

HasMemory returns a boolean if a field has been set.

### GetKb

`func (o *MaintenanceReconcileResponse) GetKb() MaintenanceReconcileResponseMemory`

GetKb returns the Kb field if non-nil, zero value otherwise.

### GetKbOk

`func (o *MaintenanceReconcileResponse) GetKbOk() (*MaintenanceReconcileResponseMemory, bool)`

GetKbOk returns a tuple with the Kb field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetKb

`func (o *MaintenanceReconcileResponse) SetKb(v MaintenanceReconcileResponseMemory)`

SetKb sets Kb field to given value.

### HasKb

`func (o *MaintenanceReconcileResponse) HasKb() bool`

HasKb returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


