
# CodeBuildRequest


## Properties

Name | Type
------------ | -------------
`path` | string
`project` | string
`embeddingCommand` | string
`force` | boolean

## Example

```typescript
import type { CodeBuildRequest } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "path": null,
  "project": null,
  "embeddingCommand": null,
  "force": null,
} satisfies CodeBuildRequest

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeBuildRequest
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


