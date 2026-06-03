
# EntitySearchResponseEntitiesInner


## Properties

Name | Type
------------ | -------------
`entity` | string
`kind` | string
`summary` | string
`score` | number

## Example

```typescript
import type { EntitySearchResponseEntitiesInner } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "entity": null,
  "kind": null,
  "summary": null,
  "score": null,
} satisfies EntitySearchResponseEntitiesInner

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as EntitySearchResponseEntitiesInner
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


