
# EntitySearchResponse


## Properties

Name | Type
------------ | -------------
`entities` | [Array&lt;EntitySearchResponseEntitiesInner&gt;](EntitySearchResponseEntitiesInner.md)
`nextCursor` | string

## Example

```typescript
import type { EntitySearchResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "entities": null,
  "nextCursor": null,
} satisfies EntitySearchResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as EntitySearchResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


