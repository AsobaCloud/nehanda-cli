
# MaintenanceClearResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`project` | string
`chunksDeleted` | number

## Example

```typescript
import type { MaintenanceClearResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "project": null,
  "chunksDeleted": null,
} satisfies MaintenanceClearResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as MaintenanceClearResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


