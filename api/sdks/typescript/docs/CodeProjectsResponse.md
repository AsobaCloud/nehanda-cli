
# CodeProjectsResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`projects` | [Array&lt;CodeProject&gt;](CodeProject.md)
`nextCursor` | string

## Example

```typescript
import type { CodeProjectsResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": null,
  "projects": null,
  "nextCursor": null,
} satisfies CodeProjectsResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeProjectsResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


