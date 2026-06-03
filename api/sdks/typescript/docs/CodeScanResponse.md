
# CodeScanResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`skipped` | boolean
`project` | string
`files` | number
`inspected` | number

## Example

```typescript
import type { CodeScanResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "skipped": null,
  "project": null,
  "files": null,
  "inspected": null,
} satisfies CodeScanResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeScanResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


