
# SearchResponse


## Properties

Name | Type
------------ | -------------
`hits` | [Array&lt;SearchHit&gt;](SearchHit.md)
`nextCursor` | string
`totalHits` | number
`fusionModeUsed` | string

## Example

```typescript
import type { SearchResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "hits": null,
  "nextCursor": null,
  "totalHits": null,
  "fusionModeUsed": null,
} satisfies SearchResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as SearchResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


