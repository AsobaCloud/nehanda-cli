
# CapabilitiesResponse


## Properties

Name | Type
------------ | -------------
`capabilities` | Array&lt;string&gt;
`version` | string

## Example

```typescript
import type { CapabilitiesResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "capabilities": [memory, search, index],
  "version": null,
} satisfies CapabilitiesResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as CapabilitiesResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


