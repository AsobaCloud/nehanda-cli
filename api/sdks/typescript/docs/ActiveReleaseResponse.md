
# ActiveReleaseResponse


## Properties

Name | Type
------------ | -------------
`releaseId` | number
`name` | string
`state` | string
`promotedAt` | string

## Example

```typescript
import type { ActiveReleaseResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "releaseId": null,
  "name": null,
  "state": null,
  "promotedAt": null,
} satisfies ActiveReleaseResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as ActiveReleaseResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


