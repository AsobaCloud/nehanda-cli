
# IngestStatusResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`queue` | [IngestStatusResponseQueue](IngestStatusResponseQueue.md)
`workers` | [IngestStatusResponseWorkers](IngestStatusResponseWorkers.md)
`recent` | [Array&lt;IngestStatusResponseRecentInner&gt;](IngestStatusResponseRecentInner.md)

## Example

```typescript
import type { IngestStatusResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "queue": null,
  "workers": null,
  "recent": null,
} satisfies IngestStatusResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as IngestStatusResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


