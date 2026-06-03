
# PipelineStatusResponse


## Properties

Name | Type
------------ | -------------
`state` | string
`queueDepth` | number
`activeJobs` | Array&lt;object&gt;
`queue` | [PipelineStatusResponseQueue](PipelineStatusResponseQueue.md)

## Example

```typescript
import type { PipelineStatusResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "state": null,
  "queueDepth": null,
  "activeJobs": null,
  "queue": null,
} satisfies PipelineStatusResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as PipelineStatusResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


