
# HealthResponse


## Properties

Name | Type
------------ | -------------
`status` | string
`db2Ok` | boolean
`db2KbTablesOk` | boolean
`pgvecOk` | boolean
`pgvecCollectionOk` | boolean
`embedOk` | boolean
`embedCommand` | string
`chunkCount` | number
`embeddingCount` | number
`warnings` | Array&lt;string&gt;

## Example

```typescript
import type { HealthResponse } from '@aimee/kb-client'

// TODO: Update the object below with actual values
const example = {
  "status": null,
  "db2Ok": null,
  "db2KbTablesOk": null,
  "pgvecOk": null,
  "pgvecCollectionOk": null,
  "embedOk": null,
  "embedCommand": null,
  "chunkCount": null,
  "embeddingCount": null,
  "warnings": null,
} satisfies HealthResponse

console.log(example)

// Convert the instance to a JSON string
const exampleJSON: string = JSON.stringify(example)
console.log(exampleJSON)

// Parse the JSON string back to an object
const exampleParsed = JSON.parse(exampleJSON) as HealthResponse
console.log(exampleParsed)
```

[[Back to top]](#) [[Back to API list]](../README.md#api-endpoints) [[Back to Model list]](../README.md#models) [[Back to README]](../README.md)


