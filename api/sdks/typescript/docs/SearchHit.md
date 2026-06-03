
# SearchHit


## Properties

Name | Type
------------ | -------------
`artifactId` | string
`score` | number
`kind` | string
`excerpt` | string
`citations` | [Array&lt;SearchHitCitationsInner&gt;](SearchHitCitationsInner.md)

## Example

```typescript
import type { SearchHit } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "artifactId": null,
  "score": null,
  "kind": null,
  "excerpt": null,
  "citations": null,
} satisfies SearchHit

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as SearchHit
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


