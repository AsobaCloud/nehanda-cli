
# DocMetadataResponse


## Properties

Name | Type
------------ | -------------
`id` | number
`filename` | string
`contentHash` | string
`converter` | string
`converterVersion` | string
`scope` | string
`state` | string
`reviewNeeded` | boolean
`createdAt` | Date

## Example

```typescript
import type { DocMetadataResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "id": null,
  "filename": null,
  "contentHash": null,
  "converter": null,
  "converterVersion": null,
  "scope": null,
  "state": null,
  "reviewNeeded": null,
  "createdAt": null,
} satisfies DocMetadataResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as DocMetadataResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


