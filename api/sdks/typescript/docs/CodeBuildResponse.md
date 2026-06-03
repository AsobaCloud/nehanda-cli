
# CodeBuildResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`project` | string
`filesScanned` | number
`filesIndexed` | number
`filesSkipped` | number
`filesRemoved` | number
`chunksAdded` | number
`chunksRemoved` | number
`embeddingsAdded` | number

## Example

```typescript
import type { CodeBuildResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "project": null,
  "filesScanned": null,
  "filesIndexed": null,
  "filesSkipped": null,
  "filesRemoved": null,
  "chunksAdded": null,
  "chunksRemoved": null,
  "embeddingsAdded": null,
} satisfies CodeBuildResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeBuildResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


