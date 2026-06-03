# MaintenanceReconcileRequest

## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**DryRun** | Pointer to **bool** |  | [optional] [default to false]

## Methods

### NewMaintenanceReconcileRequest

`func NewMaintenanceReconcileRequest() *MaintenanceReconcileRequest`

NewMaintenanceReconcileRequest instantiates a new MaintenanceReconcileRequest object
This constructor will assign default values to properties that have it defined,
and makes sure properties required by API are set, but the set of arguments
will change when the set of required properties is changed

### NewMaintenanceReconcileRequestWithDefaults

`func NewMaintenanceReconcileRequestWithDefaults() *MaintenanceReconcileRequest`

NewMaintenanceReconcileRequestWithDefaults instantiates a new MaintenanceReconcileRequest object
This constructor will only assign default values to properties that have it defined,
but it doesn't guarantee that properties required by API are set

### GetDryRun

`func (o *MaintenanceReconcileRequest) GetDryRun() bool`

GetDryRun returns the DryRun field if non-nil, zero value otherwise.

### GetDryRunOk

`func (o *MaintenanceReconcileRequest) GetDryRunOk() (*bool, bool)`

GetDryRunOk returns a tuple with the DryRun field if it's non-nil, zero value otherwise
and a boolean to check if the value has been set.

### SetDryRun

`func (o *MaintenanceReconcileRequest) SetDryRun(v bool)`

SetDryRun sets DryRun field to given value.

### HasDryRun

`func (o *MaintenanceReconcileRequest) HasDryRun() bool`

HasDryRun returns a boolean if a field has been set.


[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


