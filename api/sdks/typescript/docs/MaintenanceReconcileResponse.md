
# MaintenanceReconcileResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`rc` | number
`dryRun` | boolean
`memory` | [MaintenanceReconcileResponseMemory](MaintenanceReconcileResponseMemory.md)
`kb` | [MaintenanceReconcileResponseMemory](MaintenanceReconcileResponseMemory.md)

## Example

```typescript
import type { MaintenanceReconcileResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "rc": null,
  "dryRun": null,
  "memory": null,
  "kb": null,
} satisfies MaintenanceReconcileResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as MaintenanceReconcileResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


