
# ArtifactLinksResponse


## Properties

Name | Type
------------ | -------------
`artifactId` | string
`links` | [Array&lt;ArtifactLinksResponseLinksInner&gt;](ArtifactLinksResponseLinksInner.md)

## Example

```typescript
import type { ArtifactLinksResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "artifactId": null,
  "links": null,
} satisfies ArtifactLinksResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as ArtifactLinksResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


