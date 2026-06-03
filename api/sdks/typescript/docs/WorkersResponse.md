
# WorkersResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`configured` | number
`slots` | Array&lt;object&gt;
`threads` | Array&lt;object&gt;
`background` | Array&lt;object&gt;

## Example

```typescript
import type { WorkersResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": ok,
  "configured": null,
  "slots": null,
  "threads": null,
  "background": null,
} satisfies WorkersResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as WorkersResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


