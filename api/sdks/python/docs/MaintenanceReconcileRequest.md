# MaintenanceReconcileRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**dry_run** | **bool** |  | [optional] [default to False]

## Example

```python
from aimee_kb.models.maintenance_reconcile_request import MaintenanceReconcileRequest

# TODO update the JSON string below
json = "{}"
# create an instance of MaintenanceReconcileRequest from a JSON string
maintenance_reconcile_request_instance = MaintenanceReconcileRequest.from_json(json)
# print the JSON string representation of the object
print(MaintenanceReconcileRequest.to_json())

# convert the object into a dict
maintenance_reconcile_request_dict = maintenance_reconcile_request_instance.to_dict()
# create an instance of MaintenanceReconcileRequest from a dict
maintenance_reconcile_request_from_dict = MaintenanceReconcileRequest.from_dict(maintenance_reconcile_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


