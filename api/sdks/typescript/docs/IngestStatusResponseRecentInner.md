
# IngestStatusResponseRecentInner


## Properties

Name | Type
------------ | -------------
`project` | string
`status` | string
`completedAt` | string
`filesIndexed` | number
`chunksAdded` | number
`error` | string

## Example

```typescript
import type { IngestStatusResponseRecentInner } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "project": null,
  "status": null,
  "completedAt": null,
  "filesIndexed": null,
  "chunksAdded": null,
  "error": null,
} satisfies IngestStatusResponseRecentInner

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as IngestStatusResponseRecentInner
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


