
# DocsManifestRequest


## Properties

Name | Type
------------ | -------------
`scope` | string
`docs` | [Array&lt;DocsManifestRequestDocsInner&gt;](DocsManifestRequestDocsInner.md)

## Example

```typescript
import type { DocsManifestRequest } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "scope": null,
  "docs": null,
} satisfies DocsManifestRequest

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as DocsManifestRequest
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


