# Vulkan Renderer Config Keys

The client now loads `/rendering/vulkan.config` when the active renderer id contains `vulkan`.
If the file is missing, it falls back to `/rendering/opengl.config`.

## Supported keys

- `presentMode`: `mailbox`, `fifo`, `immediate`, `fifo_relaxed`
- `framesInFlight`: integer (`2..4`, clamped)
- `uploadBufferMiB` / `uploadBufferMB`: integer MiB (`0` disables upload/scratch buffers)
- `swapchainImages`: integer (`0` = auto)
- `staticCommandBuffers`: boolean (default `true`, pre-records swapchain command buffers)
- `enablePipelineCache`: boolean (default `true`)
- `pipelineCachePath`: string path (`""` uses platform default such as `$HOME/.cache/openstarbound/vulkan.pipeline.cache`)
- `enableTransferQueue`: boolean (default `true`, requires restart to rebuild queue families)
- `preferDiscreteGpu`: boolean
- `preferredGpuName`: case-insensitive substring filter

## Environment variable overrides

- `STAR_VK_PRESENT_MODE`
- `STAR_VK_FRAMES_IN_FLIGHT`
- `STAR_VK_UPLOAD_BUFFER_MB`
- `STAR_VK_SWAPCHAIN_IMAGES`
- `STAR_VK_STATIC_COMMAND_BUFFERS`
- `STAR_VK_ENABLE_PIPELINE_CACHE`
- `STAR_VK_PIPELINE_CACHE_PATH`
- `STAR_VK_ENABLE_TRANSFER_QUEUE`
- `STAR_VK_PREFER_DISCRETE`
- `STAR_VK_GPU_NAME`
