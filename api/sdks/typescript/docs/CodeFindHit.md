
# CodeFindHit


## Properties

Name | Type
------------ | -------------
`project` | string
`filePath` | string
`line` | number
`kind` | string

## Example

```typescript
import type { CodeFindHit } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "project": null,
  "filePath": null,
  "line": null,
  "kind": null,
} satisfies CodeFindHit

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CodeFindHit
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


