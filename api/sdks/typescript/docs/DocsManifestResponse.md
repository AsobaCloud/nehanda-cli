
# DocsManifestResponse


## Properties

Name | Type
------------ | -------------
`missing` | [Array&lt;DocsManifestResponseMissingInner&gt;](DocsManifestResponseMissingInner.md)
`total` | number
`present` | number
`missingCount` | number

## Example

```typescript
import type { DocsManifestResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "missing": null,
  "total": null,
  "present": null,
  "missingCount": null,
} satisfies DocsManifestResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as DocsManifestResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


