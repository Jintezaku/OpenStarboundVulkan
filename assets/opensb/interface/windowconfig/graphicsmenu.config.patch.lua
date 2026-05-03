-- gadgets
local function jcopy(base) return sb.jsonMerge(base, {}) end

local function clone(base, a, b)
  local copy = jcopy(base[a])
  base[b] = copy
  return copy
end

local function shift(thing, x, y)
  thing.position[1] = thing.position[1] + (tonumber(x) or 0)
  thing.position[2] = thing.position[2] + (tonumber(y) or 0)
  return thing
end

local function moveto(thing, otherthing)
  thing.position[1] = otherthing.position[1]
  thing.position[2] = otherthing.position[2]
  return thing
end

-- patch function, called by the game
function patch(config)
  local layout = config.paneLayout
  layout.panefeature.positionLocked = false
  layout.panefeature.anchor = "center"
  if layout.bgShine then
    layout.bgShine.zlevel = -10
  end
  for i = 1, 32 do config.zoomList[i] = i end
  -- Create the camera pan speed widgets
  shift(clone(layout, "zoomLabel", "cameraSpeedLabel"), 100).value = "CAMERA PAN SPEED"
  shift(clone(layout, "zoomSlider", "cameraSpeedSlider"), 100)
  shift(clone(layout, "zoomValueLabel", "cameraSpeedValueLabel"), 100)
  config.cameraSpeedList = jarray()
  for i = 1, 50 do config.cameraSpeedList[i] = i / 10 end

  -- Create the interface scale widgets
  shift(clone(layout, "zoomLabel", "interfaceScaleLabel"), 0, 28).value = "INTERFACE SCALE"
  shift(clone(layout, "zoomSlider", "interfaceScaleSlider"), 0, 28)
  shift(clone(layout, "zoomValueLabel", "interfaceScaleValueLabel"), 0, 28)
  config.interfaceScaleList = {0} -- 0 = AUTO!
  for i = 1, 17 do config.interfaceScaleList[i + 1] = 0.75 + i / 4 end

  -- Create anti-aliasing toggle
  local antiAliasingLabel = shift(clone(layout, "multiTextureLabel", "antiAliasingLabel"), 88)
  antiAliasingLabel.value = "SUPERSAMPLED AA"
  antiAliasingLabel.fontSize = 7
  shift(clone(layout, "multiTextureCheckbox", "antiAliasingCheckbox"), 99)
  -- Create new lighting toggle
  shift(clone(layout, "multiTextureLabel", "newLightingLabel"), 0, -11).value = "NEW LIGHTING"
  shift(clone(layout, "multiTextureCheckbox", "newLightingCheckbox"), 0, -11)
  -- Create hardware cursor toggle
  local hardwareCursorLabel = shift(clone(layout, "multiTextureLabel", "hardwareCursorLabel"), 88, -11)
  hardwareCursorLabel.value = "HARDWARE CURSOR"
  hardwareCursorLabel.fontSize = 7
  shift(clone(layout, "multiTextureCheckbox", "hardwareCursorCheckbox"), 99, -11)

  -- Create advanced renderer toggles
  shift(clone(layout, "multiTextureLabel", "vsyncLabel"), 0, -22).value = "VSYNC"
  shift(clone(layout, "multiTextureCheckbox", "vsyncCheckbox"), 0, -22)

  local vulkanLowLatencyLabel = shift(clone(layout, "multiTextureLabel", "vulkanLowLatencyLabel"), 88, -22)
  vulkanLowLatencyLabel.value = "LOW LATENCY"
  vulkanLowLatencyLabel.fontSize = 7
  shift(clone(layout, "multiTextureCheckbox", "vulkanLowLatencyCheckbox"), 99, -22)

  shift(clone(layout, "multiTextureLabel", "vulkanPipelineCacheLabel"), 0, -33).value = "VULKAN PIPELINE CACHE"
  shift(clone(layout, "multiTextureCheckbox", "vulkanPipelineCacheCheckbox"), 0, -33)

  local vulkanTransferQueueLabel = shift(clone(layout, "multiTextureLabel", "vulkanTransferQueueLabel"), 88, -33)
  vulkanTransferQueueLabel.value = "XFER QUEUE"
  vulkanTransferQueueLabel.fontSize = 7
  shift(clone(layout, "multiTextureCheckbox", "vulkanTransferQueueCheckbox"), 99, -33)

  shift(clone(layout, "multiTextureLabel", "vulkanStaticCommandBuffersLabel"), 0, -44).value = "STATIC COMMAND BUFFERS"
  shift(clone(layout, "multiTextureCheckbox", "vulkanStaticCommandBuffersCheckbox"), 0, -44)
  
  -- Create shader menu button
  shift(moveto(clone(layout, "accept", "showShadersMenu"), layout.interfaceScaleSlider), 112, -2).caption = "Shaders"
  

  shift(layout.title, 0, 24)
  shift(layout.resLabel, 0, 28)
  shift(layout.resSlider, 0, 28)
  shift(layout.resValueLabel, 0, 28)
  return config
end
