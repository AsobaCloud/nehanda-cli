
# CodeSearchResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`hits` | [Array&lt;CodeSearchHit&gt;](CodeSearchHit.md)
`nextCursor` | string

## Example

```typescript
import type { CodeSearchResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": null,
  "hits": null,
  "nextCursor": null,
} satisfies CodeSearchResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeSearchResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


