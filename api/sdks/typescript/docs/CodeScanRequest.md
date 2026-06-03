
# CodeScanRequest


## Properties

Name | Type
------------ | -------------
`project` | string
`rootPath` | string
`force` | boolean

## Example

```typescript
import type { CodeScanRequest } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "project": null,
  "rootPath": null,
  "force": null,
} satisfies CodeScanRequest

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeScanRequest
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


