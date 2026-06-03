
# EntitySearchRequest


## Properties

Name | Type
------------ | -------------
`query` | string
`limit` | number
`cursor` | string

## Example

```typescript
import type { EntitySearchRequest } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "query": null,
  "limit": null,
  "cursor": null,
} satisfies EntitySearchRequest

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as EntitySearchRequest
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


