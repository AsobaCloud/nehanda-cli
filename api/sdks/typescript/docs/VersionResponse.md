
# VersionResponse


## Properties

Name | Type
------------ | -------------
`version` | string
`service` | string

## Example

```typescript
import type { VersionResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "version": 0.2.0,
  "service": aimee-kb,
} satisfies VersionResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as VersionResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


