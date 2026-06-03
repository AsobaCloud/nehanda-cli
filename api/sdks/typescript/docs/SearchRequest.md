
# SearchRequest


## Properties

Name | Type
------------ | -------------
`query` | string
`project` | string
`scopeKind` | string
`scopeId` | string
`maxResults` | number
`fusionMode` | string
`cursor` | string

## Example

```typescript
import type { SearchRequest } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "query": null,
  "project": null,
  "scopeKind": null,
  "scopeId": null,
  "maxResults": null,
  "fusionMode": null,
  "cursor": null,
} satisfies SearchRequest

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as SearchRequest
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


