

# JobStatusResponse


## Properties

| Name | Type | Description | Notes |
|------------ | ------------- | ------------- | -------------|
|**id** | **Long** |  |  |
|**kind** | **String** |  |  |
|**documentId** | **Long** |  |  |
|**project** | **String** |  |  |
|**status** | [**StatusEnum**](#StatusEnum) |  |  |
|**attempts** | **Integer** |  |  |
|**lastError** | **String** |  |  [optional] |
|**claimedBy** | **String** |  |  [optional] |
|**claimedAt** | **String** |  |  [optional] |
|**createdAt** | **String** |  |  [optional] |
|**updatedAt** | **String** |  |  [optional] |



## Enum: StatusEnum

| Name | Value |
|---- | -----|
| PENDING | &quot;pending&quot; |
| RUNNING | &quot;running&quot; |
| DONE | &quot;done&quot; |
| FAILED | &quot;failed&quot; |



