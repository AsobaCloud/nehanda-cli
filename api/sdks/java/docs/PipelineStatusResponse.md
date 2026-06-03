

# PipelineStatusResponse


## Properties

| Name | Type | Description | Notes |
|------------ | ------------- | ------------- | -------------|
|**state** | [**StateEnum**](#StateEnum) |  |  [optional] |
|**queueDepth** | **Integer** |  |  [optional] |
|**activeJobs** | **List&lt;Object&gt;** |  |  [optional] |
|**queue** | [**PipelineStatusResponseQueue**](PipelineStatusResponseQueue.md) |  |  [optional] |



## Enum: StateEnum

| Name | Value |
|---- | -----|
| IDLE | &quot;idle&quot; |
| RUNNING | &quot;running&quot; |
| FAILED | &quot;failed&quot; |



