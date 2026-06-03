
# DrainResponse


## Properties

Name | Type
------------ | -------------
`state` | string
`processed` | number
`pending` | number
`running` | number
`done` | number
`failed` | number
`total` | number

## Example

```typescript
import type { DrainResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "state": null,
  "processed": null,
  "pending": null,
  "running": null,
  "done": null,
  "failed": null,
  "total": null,
} satisfies DrainResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as DrainResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


