
# ReviewQueueResponse


## Properties

Name | Type
------------ | -------------
`docs` | [Array&lt;DocMetadataResponse&gt;](DocMetadataResponse.md)
`nextCursor` | number

## Example

```typescript
import type { ReviewQueueResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "docs": null,
  "nextCursor": null,
} satisfies ReviewQueueResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as ReviewQueueResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


