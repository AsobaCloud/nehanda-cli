
# CodeStructureResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`definitions` | [Array&lt;CodeDefinition&gt;](CodeDefinition.md)

## Example

```typescript
import type { CodeStructureResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": null,
  "definitions": null,
} satisfies CodeStructureResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeStructureResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


