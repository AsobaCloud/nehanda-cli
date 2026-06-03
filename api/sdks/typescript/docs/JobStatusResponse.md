
# JobStatusResponse


## Properties

Name | Type
------------ | -------------
`id` | number
`kind` | string
`documentId` | number
`project` | string
`status` | string
`attempts` | number
`lastError` | string
`claimedBy` | string
`claimedAt` | string
`createdAt` | string
`updatedAt` | string

## Example

```typescript
import type { JobStatusResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "id": null,
  "kind": null,
  "documentId": null,
  "project": null,
  "status": null,
  "attempts": null,
  "lastError": null,
  "claimedBy": null,
  "claimedAt": null,
  "createdAt": null,
  "updatedAt": null,
} satisfies JobStatusResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as JobStatusResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


