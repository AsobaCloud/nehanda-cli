
# ArtifactResponse


## Properties

Name | Type
------------ | -------------
`id` | string
`kind` | string
`state` | string
`scopeKind` | string
`scopeId` | string
`confidence` | number
`payload` | object
`citations` | [Array&lt;SearchHitCitationsInner&gt;](SearchHitCitationsInner.md)
`updatedAt` | Date

## Example

```typescript
import type { ArtifactResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "id": null,
  "kind": null,
  "state": null,
  "scopeKind": null,
  "scopeId": null,
  "confidence": null,
  "payload": null,
  "citations": null,
  "updatedAt": null,
} satisfies ArtifactResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as ArtifactResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


