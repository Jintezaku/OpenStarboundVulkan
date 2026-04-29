#include "StarRenderer_vulkan.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <set>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.h"

#include "StarCasting.hpp"
#include "StarFile.hpp"
#include "StarLogging.hpp"

namespace Star {

namespace {

#ifdef NDEBUG
constexpr bool EnableValidationLayers = false;
#else
constexpr bool EnableValidationLayers = true;
#endif

char const* ValidationLayerName = "VK_LAYER_KHRONOS_validation";
constexpr uint32_t DefaultFramesInFlight = 2;
constexpr uint32_t MinFramesInFlight = 2;
constexpr uint32_t MaxFramesInFlight = 4;
constexpr VkDeviceSize DefaultUploadBufferSize = 64ull * 1024ull * 1024ull;
constexpr VkDeviceSize MaxUploadBufferSize = 512ull * 1024ull * 1024ull;

struct QueueFamilySelection {
  Maybe<uint32_t> graphicsFamily;
  Maybe<uint32_t> presentFamily;
  Maybe<uint32_t> transferFamily;

  bool complete() const {
    return graphicsFamily && presentFamily;
  }
};

String vkResultToString(VkResult result) {
  switch (result) {
  case VK_SUCCESS: return "VK_SUCCESS";
  case VK_NOT_READY: return "VK_NOT_READY";
  case VK_TIMEOUT: return "VK_TIMEOUT";
  case VK_EVENT_SET: return "VK_EVENT_SET";
  case VK_EVENT_RESET: return "VK_EVENT_RESET";
  case VK_INCOMPLETE: return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
  default:
    return "VkResultUnknown";
  }
}

void checkVk(VkResult result, char const* context) {
  if (result != VK_SUCCESS) {
    throw RendererException::format("{} failed: {}", context, vkResultToString(result));
  }
}

Maybe<VkPresentModeKHR> presentModeFromString(String const& presentModeName) {
  auto modeName = presentModeName.trim().toLower();
  if (modeName == "fifo" || modeName == "vsync")
    return VK_PRESENT_MODE_FIFO_KHR;
  if (modeName == "mailbox")
    return VK_PRESENT_MODE_MAILBOX_KHR;
  if (modeName == "immediate" || modeName == "tearing")
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  if (modeName == "fifo_relaxed" || modeName == "fiforelaxed" || modeName == "adaptive")
    return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  return {};
}

String presentModeToString(VkPresentModeKHR mode) {
  switch (mode) {
  case VK_PRESENT_MODE_FIFO_KHR:
    return "fifo";
  case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    return "fifo_relaxed";
  case VK_PRESENT_MODE_MAILBOX_KHR:
    return "mailbox";
  case VK_PRESENT_MODE_IMMEDIATE_KHR:
    return "immediate";
  default:
    return "unknown";
  }
}

Maybe<bool> boolFromString(String const& boolText) {
  auto normalized = boolText.trim().toLower();
  if (normalized.empty())
    return {};

  if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    return true;
  if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
    return false;
  return {};
}

Maybe<uint32_t> parseUint(String const& uintText) {
  auto normalized = uintText.trim();
  if (normalized.empty())
    return {};

  errno = 0;
  char* parseEnd = nullptr;
  auto parsed = std::strtoul(normalized.utf8Ptr(), &parseEnd, 10);
  if (errno != 0 || parseEnd == normalized.utf8Ptr() || (parseEnd && *parseEnd != '\0'))
    return {};
  if (parsed > std::numeric_limits<uint32_t>::max())
    return {};

  return static_cast<uint32_t>(parsed);
}

String defaultPipelineCachePath() {
  if (auto xdgCacheHome = std::getenv("XDG_CACHE_HOME")) {
    auto cacheRoot = String(xdgCacheHome).trim();
    if (!cacheRoot.empty())
      return File::relativeTo(cacheRoot, "openstarbound/vulkan.pipeline.cache");
  }

  if (auto home = std::getenv("HOME")) {
    auto homePath = String(home).trim();
    if (!homePath.empty())
      return File::relativeTo(homePath, ".cache/openstarbound/vulkan.pipeline.cache");
  }

  return "vulkan.pipeline.cache";
}

constexpr uint32_t VulkanVertexShaderCode[] = {
  119734787, 65536, 524299, 49, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 720911, 0, 4, 1852399981, 0, 11, 33, 42, 43, 45, 47, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613, 9, 6513774, 262149, 11, 1867542121, 115, 393221, 13, 1752397136, 1936617283, 1953390964, 115, 393222, 13, 0, 1701995379, 1767075429, 25978, 196613, 15, 25456, 393221, 31, 1348430951, 1700164197, 2019914866, 0, 393222, 31, 0, 1348430951, 1953067887, 7237481, 458758, 31, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 31, 2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 31, 3, 1130327143, 1147956341, 1635021673, 6644590, 196613, 33, 0, 262149, 42, 1734439526, 22101, 262149, 43, 1448439401, 0, 327685, 45, 1734439526, 1869377347, 114, 262149, 47, 1866690153, 7499628, 262215, 11, 30, 0, 196679, 13, 2, 327752, 13, 0, 35, 0, 196679, 31, 2, 327752, 31, 0, 11, 0, 327752, 31, 1, 11, 1, 327752, 31, 2, 11, 3, 327752, 31, 3, 11, 4, 262215, 42, 30, 0, 262215, 43, 30, 1, 262215, 45, 30, 1, 262215, 47, 30, 2, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 2, 262176, 8, 7, 7, 262176, 10, 1, 7, 262203, 10, 11, 1, 196638, 13, 7, 262176, 14, 9, 13, 262203, 14, 15, 9, 262165, 16, 32, 1, 262187, 16, 17, 0, 262176, 18, 9, 7, 262187, 6, 22, 1073741824, 262187, 6, 24, 1065353216, 262167, 27, 6, 4, 262165, 28, 32, 0, 262187, 28, 29, 1, 262172, 30, 6, 29, 393246, 31, 27, 6, 30, 30, 262176, 32, 3, 31, 262203, 32, 33, 3, 262187, 6, 35, 0, 262176, 39, 3, 27, 262176, 41, 3, 7, 262203, 41, 42, 3, 262203, 10, 43, 1, 262203, 39, 45, 3, 262176, 46, 1, 27, 262203, 46, 47, 1, 327734, 2, 4, 0, 3, 131320, 5, 262203, 8, 9, 7, 262205, 7, 12, 11, 327745, 18, 19, 15, 17, 262205, 7, 20, 19, 327816, 7, 21, 12, 20, 327822, 7, 23, 21, 22, 327760, 7, 25, 24, 24, 327811, 7, 26, 23, 25, 196670, 9, 26, 262205, 7, 34, 9, 327761, 6, 36, 34, 0, 327761, 6, 37, 34, 1, 458832, 27, 38, 36, 37, 35, 24, 327745, 39, 40, 33, 17, 196670, 40, 38, 262205, 7, 44, 43, 196670, 42, 44, 262205, 27, 48, 47, 196670, 45, 48, 65789, 65592
};

constexpr uint32_t VulkanFragmentShaderCode[] = {
  119734787, 65536, 524299, 38, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 524303, 4, 4, 1852399981, 0, 17, 32, 35, 196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 327685, 9, 1131963764, 1919904879, 0, 327685, 13, 1400399220, 1819307361, 29285, 262149, 17, 1734439526, 22101, 327685, 32, 1131705711, 1919904879, 0, 327685, 35, 1734439526, 1869377347, 114, 262215, 13, 33, 0, 262215, 13, 34, 0, 262215, 17, 30, 0, 262215, 32, 30, 0, 262215, 35, 30, 1, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7, 589849, 10, 6, 1, 0, 0, 0, 1, 0, 196635, 11, 10, 262176, 12, 0, 11, 262203, 12, 13, 0, 262167, 15, 6, 2, 262176, 16, 1, 15, 262203, 16, 17, 1, 262165, 20, 32, 0, 262187, 20, 21, 3, 262176, 22, 7, 6, 262187, 6, 25, 0, 131092, 26, 262176, 31, 3, 7, 262203, 31, 32, 3, 262176, 34, 1, 7, 262203, 34, 35, 1, 327734, 2, 4, 0, 3, 131320, 5, 262203, 8, 9, 7, 262205, 11, 14, 13, 262205, 15, 18, 17, 327767, 7, 19, 14, 18, 196670, 9, 19, 327745, 22, 23, 9, 21, 262205, 6, 24, 23, 327868, 26, 27, 24, 25, 196855, 29, 0, 262394, 27, 28, 29, 131320, 28, 65788, 131320, 29, 262205, 7, 33, 9, 262205, 7, 36, 35, 327813, 7, 37, 33, 36, 196670, 32, 37, 65789, 65592
};

struct VulkanRenderVertex {
  float pos[2];
  float uv[2];
  Vec4B color;
};

class VulkanTexture : public Texture {
public:
  VulkanTexture(Vec2U size, TextureAddressing addressing, TextureFiltering filtering)
    : m_size(size), m_addressing(addressing), m_filtering(filtering) {}

  Vec2U size() const override {
    return m_size;
  }

  TextureFiltering filtering() const override {
    return m_filtering;
  }

  TextureAddressing addressing() const override {
    return m_addressing;
  }

private:
  Vec2U m_size;
  TextureAddressing m_addressing;
  TextureFiltering m_filtering;

public:
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

class VulkanTextureGroup : public TextureGroup {
public:
  VulkanTextureGroup(VulkanRenderer* renderer, TextureFiltering filtering)
    : m_renderer(renderer), m_filtering(filtering) {}

  TextureFiltering filtering() const override {
    return m_filtering;
  }

  TexturePtr create(Image const& texture) override {
    return m_renderer->createTexture(texture, TextureAddressing::Clamp, m_filtering);
  }

private:
  VulkanRenderer* m_renderer;
  TextureFiltering m_filtering;
};

class VulkanRenderBuffer : public RenderBuffer {
public:
  void set(List<RenderPrimitive>& primitives) override {
    m_primitives = primitives;
  }

  List<RenderPrimitive> const& primitives() const {
    return m_primitives;
  }

private:
  List<RenderPrimitive> m_primitives;
};

struct SwapchainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

}

struct VulkanRenderer::Impl {
  struct FrameSync {
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
  };

  struct BufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
  };

  struct TransferContext {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
  };

  struct DrawSlice {
    RefPtr<VulkanTexture> texture;
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    Maybe<RectI> scissor;
  };

  explicit Impl(SDL_Window* sdlWindow)
    : window(sdlWindow) {}

  ~Impl() {
    cleanup();
  }

  void initialize() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createDescriptorResources();
    createPipelineCache();
    createCommandPools();
    initializeTransferContext();
    createUploadResources();
    createSyncObjects();
    recreateSwapchain();
    Logger::info("Vulkan: staticCommandBuffers={}, transferQueue={}, pipelineCache={} ({})",
        useStaticCommandBuffers,
        enableTransferQueue,
        enablePipelineCache,
        pipelineCachePath.empty() ? "<disabled>" : pipelineCachePath);
  }

  void cleanup() {
    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
    }

    cleanupSwapchain();

    if (device != VK_NULL_HANDLE) {
      destroySyncObjects();
      destroyUploadResources();
      destroyBuffer(dynamicVertexBuffer);
      destroyTransferContext();
      destroyGraphicsPipeline();
      destroyLiveTextures();
      destroyDescriptorResources();
      destroyPipelineCache();

      if (transferCommandPool != VK_NULL_HANDLE && transferCommandPool != graphicsCommandPool)
        vkDestroyCommandPool(device, transferCommandPool, nullptr);
      if (graphicsCommandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
      transferCommandPool = VK_NULL_HANDLE;
      graphicsCommandPool = VK_NULL_HANDLE;

      transferQueue = VK_NULL_HANDLE;
      presentQueue = VK_NULL_HANDLE;
      graphicsQueue = VK_NULL_HANDLE;
      vkDestroyDevice(device, nullptr);
      device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
      SDL_Vulkan_DestroySurface(instance, surface, nullptr);
      surface = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
    }
  }

  void createInstance() {
    Uint32 extensionCount = 0;
    char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!sdlExtensions || extensionCount == 0)
      throw RendererException::format("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());

    std::vector<char const*> extensions(sdlExtensions, sdlExtensions + extensionCount);
    std::vector<char const*> validationLayers = collectValidationLayers();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "OpenStarbound";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "OpenStarbound";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.empty() ? nullptr : validationLayers.data();

    checkVk(vkCreateInstance(&createInfo, nullptr, &instance), "vkCreateInstance");
  }

  std::vector<char const*> collectValidationLayers() {
    std::vector<char const*> selectedLayers;
    if (!EnableValidationLayers)
      return selectedLayers;

    uint32_t layerCount = 0;
    checkVk(vkEnumerateInstanceLayerProperties(&layerCount, nullptr), "vkEnumerateInstanceLayerProperties(count)");
    if (layerCount == 0)
      return selectedLayers;

    std::vector<VkLayerProperties> availableLayers(layerCount);
    checkVk(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()), "vkEnumerateInstanceLayerProperties(list)");

    for (auto const& layer : availableLayers) {
      if (std::strcmp(layer.layerName, ValidationLayerName) == 0) {
        selectedLayers.push_back(ValidationLayerName);
        Logger::info("Vulkan: enabling validation layer '{}'", ValidationLayerName);
        break;
      }
    }

    if (selectedLayers.empty())
      Logger::warn("Vulkan: validation layer '{}' not found, continuing without it", ValidationLayerName);
    return selectedLayers;
  }

  void createSurface() {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
      throw RendererException::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
  }

  void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (deviceCount == 0)
      throw RendererException("No Vulkan physical devices were found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    int bestScore = std::numeric_limits<int>::min();
    QueueFamilySelection bestQueues;

    for (auto candidate : devices) {
      auto queueFamilies = findQueueFamilies(candidate);
      if (!queueFamilies.complete())
        continue;
      if (!hasSwapchainSupport(candidate))
        continue;

      int score = scorePhysicalDevice(candidate, queueFamilies);
      if (score > bestScore) {
        bestScore = score;
        physicalDevice = candidate;
        bestQueues = queueFamilies;
      }
    }

    if (physicalDevice == VK_NULL_HANDLE)
      throw RendererException("No Vulkan physical device with graphics+present support was found");

    graphicsQueueFamily = *bestQueues.graphicsFamily;
    presentQueueFamily = *bestQueues.presentFamily;
    transferQueueFamily = bestQueues.transferFamily.value(graphicsQueueFamily);
    if (!enableTransferQueue)
      transferQueueFamily = graphicsQueueFamily;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    auto vramGiB = (double)deviceLocalMemoryBytes(physicalDevice) / (1024.0 * 1024.0 * 1024.0);
    Logger::info("Vulkan: using GPU '{}' (API {}.{}.{}, {:.2f} GiB device-local memory)",
        properties.deviceName,
        VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion),
        vramGiB);

    Logger::info("Vulkan: queue families graphics={}, present={}, transfer={}",
        graphicsQueueFamily,
        presentQueueFamily,
        transferQueueFamily);
  }

  int scorePhysicalDevice(VkPhysicalDevice deviceHandle, QueueFamilySelection const& queues) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(deviceHandle, &properties);

    int score = 0;
    switch (properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      score += 5000;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      score += 3000;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      score += 2000;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      score += 500;
      break;
    default:
      score += 1000;
      break;
    }

    if (preferDiscreteGpu && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      score -= 2500;

    if (!preferredGpuName.empty()) {
      auto deviceName = String(properties.deviceName).toLower();
      if (deviceName.contains(preferredGpuName.toLower()))
        score += 15000;
      else
        score -= 4000;
    }

    auto deviceLocalBytes = deviceLocalMemoryBytes(deviceHandle);
    score += static_cast<int>(std::min<uint64_t>(deviceLocalBytes / (256ull * 1024ull * 1024ull), 10000ull));
    score += std::min<int>(static_cast<int>(properties.limits.maxImageDimension2D), 16384);

    if (queues.transferFamily && queues.graphicsFamily && *queues.transferFamily != *queues.graphicsFamily)
      score += 600;

    return score;
  }

  uint64_t deviceLocalMemoryBytes(VkPhysicalDevice deviceHandle) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(deviceHandle, &memoryProperties);

    uint64_t totalDeviceLocalBytes = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
      if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        totalDeviceLocalBytes += memoryProperties.memoryHeaps[i].size;
    }
    return totalDeviceLocalBytes;
  }

  QueueFamilySelection findQueueFamilies(VkPhysicalDevice deviceHandle) {
    QueueFamilySelection selection;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(deviceHandle, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
      return selection;

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(deviceHandle, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      auto queueFlags = queueFamilies[i].queueFlags;
      bool hasGraphics = (queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
      bool hasTransfer = (queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
      bool hasPresent = SDL_Vulkan_GetPresentationSupport(instance, deviceHandle, i);

      if (hasGraphics && !selection.graphicsFamily)
        selection.graphicsFamily = i;
      if (hasPresent && !selection.presentFamily)
        selection.presentFamily = i;
      if (hasTransfer) {
        if (!selection.transferFamily || !hasGraphics)
          selection.transferFamily = i;
      }

    }

    return selection;
  }

  bool hasSwapchainSupport(VkPhysicalDevice deviceHandle) {
    uint32_t extensionCount = 0;
    checkVk(vkEnumerateDeviceExtensionProperties(deviceHandle, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties(count)");
    std::vector<VkExtensionProperties> extensions(extensionCount);
    checkVk(vkEnumerateDeviceExtensionProperties(deviceHandle, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties(list)");

    bool hasSwapchainExtension = false;
    for (auto const& extension : extensions) {
      if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
        hasSwapchainExtension = true;
        break;
      }
    }
    if (!hasSwapchainExtension)
      return false;

    auto details = querySwapchainSupport(deviceHandle);
    return !details.formats.empty() && !details.presentModes.empty();
  }

  void createLogicalDevice() {
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(3);

    std::set<uint32_t> uniqueQueueFamilies{graphicsQueueFamily, presentQueueFamily, transferQueueFamily};
    for (auto queueFamilyIndex : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    char const* deviceExtensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    checkVk(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "vkCreateDevice");
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
    vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);
  }

  void createPipelineCache() {
    if (device == VK_NULL_HANDLE || !enablePipelineCache)
      return;

    ByteArray initialData;
    if (!pipelineCachePath.empty() && File::isFile(pipelineCachePath)) {
      try {
        initialData = File::readFile(pipelineCachePath);
        Logger::info("Vulkan: loaded pipeline cache data from '{}' ({} bytes)",
            pipelineCachePath,
            initialData.size());
      } catch (std::exception const& e) {
        Logger::warn("Vulkan: failed reading pipeline cache '{}': {}", pipelineCachePath, e.what());
      }
    }

    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    createInfo.initialDataSize = initialData.size();
    createInfo.pInitialData = initialData.empty() ? nullptr : initialData.ptr();

    auto createResult = vkCreatePipelineCache(device, &createInfo, nullptr, &pipelineCache);
    if (createResult == VK_SUCCESS)
      return;

    if (!initialData.empty()) {
      Logger::warn("Vulkan: pipeline cache blob was rejected ({}), recreating empty cache",
          vkResultToString(createResult));
      createInfo.initialDataSize = 0;
      createInfo.pInitialData = nullptr;
      checkVk(vkCreatePipelineCache(device, &createInfo, nullptr, &pipelineCache), "vkCreatePipelineCache(empty)");
      return;
    }

    checkVk(createResult, "vkCreatePipelineCache");
  }

  void persistPipelineCache() {
    if (device == VK_NULL_HANDLE || pipelineCache == VK_NULL_HANDLE || !enablePipelineCache || pipelineCachePath.empty())
      return;

    size_t cacheDataSize = 0;
    checkVk(vkGetPipelineCacheData(device, pipelineCache, &cacheDataSize, nullptr), "vkGetPipelineCacheData(size)");
    if (cacheDataSize == 0)
      return;

    ByteArray cacheData(cacheDataSize, 0);
    checkVk(vkGetPipelineCacheData(device, pipelineCache, &cacheDataSize, cacheData.ptr()), "vkGetPipelineCacheData(blob)");
    cacheData.resize(cacheDataSize);

    try {
      auto parentDirectory = File::dirName(pipelineCachePath);
      if (!parentDirectory.empty() && !File::isDirectory(parentDirectory))
        File::makeDirectoryRecursive(parentDirectory);
      File::overwriteFileWithRename(cacheData, pipelineCachePath);
      Logger::info("Vulkan: wrote pipeline cache '{}' ({} bytes)", pipelineCachePath, cacheData.size());
    } catch (std::exception const& e) {
      Logger::warn("Vulkan: failed writing pipeline cache '{}': {}", pipelineCachePath, e.what());
    }
  }

  void destroyPipelineCache() {
    if (device == VK_NULL_HANDLE || pipelineCache == VK_NULL_HANDLE)
      return;

    persistPipelineCache();
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    pipelineCache = VK_NULL_HANDLE;
  }

  void createCommandPools() {
    VkCommandPoolCreateInfo graphicsPoolInfo{};
    graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    graphicsPoolInfo.queueFamilyIndex = graphicsQueueFamily;

    checkVk(vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &graphicsCommandPool), "vkCreateCommandPool(graphics)");

    if (transferQueueFamily == graphicsQueueFamily) {
      transferCommandPool = graphicsCommandPool;
      return;
    }

    VkCommandPoolCreateInfo transferPoolInfo{};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    transferPoolInfo.queueFamilyIndex = transferQueueFamily;
    checkVk(vkCreateCommandPool(device, &transferPoolInfo, nullptr, &transferCommandPool), "vkCreateCommandPool(transfer)");
  }

  void initializeTransferContext() {
    if (!enableTransferQueue || device == VK_NULL_HANDLE || transferCommandPool == VK_NULL_HANDLE || transferQueue == VK_NULL_HANDLE)
      return;

    if (transferContext.commandBuffer == VK_NULL_HANDLE) {
      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.commandPool = transferCommandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;
      checkVk(vkAllocateCommandBuffers(device, &allocInfo, &transferContext.commandBuffer), "vkAllocateCommandBuffers(transfer)");
    }

    if (transferContext.fence == VK_NULL_HANDLE) {
      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      checkVk(vkCreateFence(device, &fenceInfo, nullptr, &transferContext.fence), "vkCreateFence(transfer)");
    }
  }

  void destroyTransferContext() {
    if (device == VK_NULL_HANDLE)
      return;

    if (transferContext.fence != VK_NULL_HANDLE) {
      checkVk(vkWaitForFences(device, 1, &transferContext.fence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
          "vkWaitForFences(transfer)");
      vkDestroyFence(device, transferContext.fence, nullptr);
      transferContext.fence = VK_NULL_HANDLE;
    }

    if (transferContext.commandBuffer != VK_NULL_HANDLE) {
      VkCommandPool commandPool = transferCommandPool != VK_NULL_HANDLE ? transferCommandPool : graphicsCommandPool;
      if (commandPool != VK_NULL_HANDLE)
        vkFreeCommandBuffers(device, commandPool, 1, &transferContext.commandBuffer);
      transferContext.commandBuffer = VK_NULL_HANDLE;
    }
  }

  void waitForQueuesIdle() {
    if (device == VK_NULL_HANDLE)
      return;

    if (graphicsQueue != VK_NULL_HANDLE)
      checkVk(vkQueueWaitIdle(graphicsQueue), "vkQueueWaitIdle(graphics)");
    if (presentQueue != VK_NULL_HANDLE && presentQueue != graphicsQueue)
      checkVk(vkQueueWaitIdle(presentQueue), "vkQueueWaitIdle(present)");
    if (transferQueue != VK_NULL_HANDLE && transferQueue != graphicsQueue && transferQueue != presentQueue)
      checkVk(vkQueueWaitIdle(transferQueue), "vkQueueWaitIdle(transfer)");
  }

  void waitForFrameFences() {
    if (device == VK_NULL_HANDLE || frameSyncObjects.empty())
      return;

    std::vector<VkFence> waitFences;
    waitFences.reserve(frameSyncObjects.size());
    for (auto const& frameSync : frameSyncObjects) {
      if (frameSync.inFlightFence != VK_NULL_HANDLE)
        waitFences.push_back(frameSync.inFlightFence);
    }

    if (!waitFences.empty()) {
      checkVk(vkWaitForFences(device,
          (uint32_t)waitFences.size(),
          waitFences.data(),
          VK_TRUE,
          std::numeric_limits<uint64_t>::max()),
          "vkWaitForFences(allFrames)");
    }
  }

  void createSyncObjects() {
    destroySyncObjects();

    auto inFlightFrameCount = std::clamp(framesInFlight, MinFramesInFlight, MaxFramesInFlight);
    if (inFlightFrameCount != framesInFlight) {
      Logger::warn("Vulkan: clamping requested framesInFlight {} to {}", framesInFlight, inFlightFrameCount);
      framesInFlight = inFlightFrameCount;
    }

    frameSyncObjects.clear();
    frameSyncObjects.resize(framesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& sync : frameSyncObjects) {
      checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sync.imageAvailable), "vkCreateSemaphore(imageAvailable)");
      checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sync.renderFinished), "vkCreateSemaphore(renderFinished)");
      checkVk(vkCreateFence(device, &fenceInfo, nullptr, &sync.inFlightFence), "vkCreateFence(inFlight)");
    }

    currentFrame = 0;
  }

  void destroySyncObjects() {
    if (device == VK_NULL_HANDLE)
      return;

    for (auto& sync : frameSyncObjects) {
      if (sync.imageAvailable != VK_NULL_HANDLE)
        vkDestroySemaphore(device, sync.imageAvailable, nullptr);
      if (sync.renderFinished != VK_NULL_HANDLE)
        vkDestroySemaphore(device, sync.renderFinished, nullptr);
      if (sync.inFlightFence != VK_NULL_HANDLE)
        vkDestroyFence(device, sync.inFlightFence, nullptr);
    }
    frameSyncObjects.clear();
    imageInFlightFences.clear();
  }

  void recreateSyncObjects() {
    if (device == VK_NULL_HANDLE)
      return;

    waitForFrameFences();
    waitForQueuesIdle();
    createSyncObjects();
    imageInFlightFences.assign(swapchainImages.size(), VK_NULL_HANDLE);
    frameActive = false;
    activeFrame = 0;
  }

  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
      bool typeMatches = (typeFilter & (1u << i)) != 0;
      bool propertiesMatch = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
      if (typeMatches && propertiesMatch)
        return i;
    }

    throw RendererException::format("Could not find suitable Vulkan memory type for property mask {}", properties);
  }

  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties, BufferResource& destination, char const* label) {
    if (size == 0)
      return;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    checkVk(vkCreateBuffer(device, &bufferInfo, nullptr, &destination.buffer), label);

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, destination.buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, memoryProperties);

    checkVk(vkAllocateMemory(device, &allocInfo, nullptr, &destination.memory), "vkAllocateMemory(buffer)");
    checkVk(vkBindBufferMemory(device, destination.buffer, destination.memory, 0), "vkBindBufferMemory");

    destination.size = size;
  }

  void destroyBuffer(BufferResource& buffer) {
    if (device == VK_NULL_HANDLE)
      return;

    if (buffer.mapped) {
      vkUnmapMemory(device, buffer.memory);
      buffer.mapped = nullptr;
    }

    if (buffer.buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, buffer.buffer, nullptr);
      buffer.buffer = VK_NULL_HANDLE;
    }

    if (buffer.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, buffer.memory, nullptr);
      buffer.memory = VK_NULL_HANDLE;
    }

    buffer.size = 0;
  }

  void createUploadResources() {
    if (uploadBufferSize == 0)
      return;

    auto clampedUploadBufferSize = std::clamp(uploadBufferSize, (VkDeviceSize)(4ull * 1024ull * 1024ull), MaxUploadBufferSize);
    if (clampedUploadBufferSize != uploadBufferSize) {
      Logger::warn("Vulkan: clamping uploadBufferSize {} bytes to {}", uploadBufferSize, clampedUploadBufferSize);
      uploadBufferSize = clampedUploadBufferSize;
    }

    constexpr VkMemoryPropertyFlags uploadMemoryCandidates[] = {
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    };

    for (VkDeviceSize candidateSize = uploadBufferSize; candidateSize >= 4ull * 1024ull * 1024ull; candidateSize /= 2) {
      try {
        bool uploadAllocated = false;
        for (auto memoryProperties : uploadMemoryCandidates) {
          try {
            createBuffer(candidateSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                memoryProperties,
                uploadBuffer,
                "vkCreateBuffer(upload)");
            uploadBufferMemoryProperties = memoryProperties;
            uploadAllocated = true;
            break;
          } catch (std::exception const&) {
            destroyBuffer(uploadBuffer);
          }
        }

        if (!uploadAllocated)
          throw RendererException("Could not allocate host-visible upload buffer for any memory property strategy");

        checkVk(vkMapMemory(device, uploadBuffer.memory, 0, uploadBuffer.size, 0, &uploadBuffer.mapped), "vkMapMemory(upload)");

        createBuffer(candidateSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            gpuScratchBuffer,
            "vkCreateBuffer(gpuScratch)");

        uploadBufferSize = candidateSize;
        Logger::info("Vulkan: allocated upload buffer {} MiB ({}{} host memory) and {} MiB GPU scratch buffer",
            uploadBuffer.size / (1024 * 1024),
            (uploadBufferMemoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "coherent" : "non-coherent",
            (uploadBufferMemoryProperties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? ", cached" : "",
            gpuScratchBuffer.size / (1024 * 1024));
        return;
      } catch (std::exception const& e) {
        Logger::warn("Vulkan: upload buffer allocation at {} MiB failed: {}", candidateSize / (1024 * 1024), e.what());
        destroyUploadResources();
      }
    }

    uploadBufferSize = 0;
    Logger::warn("Vulkan: upload/GPU scratch buffers disabled because allocation failed at all fallback sizes");
  }

  void destroyUploadResources() {
    destroyBuffer(gpuScratchBuffer);
    destroyBuffer(uploadBuffer);
    uploadBufferMemoryProperties = 0;
  }

  void recreateUploadResources() {
    if (device == VK_NULL_HANDLE)
      return;

    waitForQueuesIdle();
    destroyUploadResources();
    createUploadResources();
  }

  VkShaderModule createShaderModule(uint32_t const* code, size_t wordCount, char const* context) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = wordCount * sizeof(uint32_t);
    createInfo.pCode = code;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), context);
    return shaderModule;
  }

  void createDescriptorResources() {
    if (textureDescriptorSetLayout != VK_NULL_HANDLE)
      return;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;
    checkVk(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureDescriptorSetLayout), "vkCreateDescriptorSetLayout(texture)");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 16384;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 16384;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    checkVk(vkCreateDescriptorPool(device, &poolInfo, nullptr, &textureDescriptorPool), "vkCreateDescriptorPool(texture)");
  }

  void destroyDescriptorResources() {
    if (device == VK_NULL_HANDLE)
      return;

    if (textureDescriptorPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, textureDescriptorPool, nullptr);
      textureDescriptorPool = VK_NULL_HANDLE;
    }
    if (textureDescriptorSetLayout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, nullptr);
      textureDescriptorSetLayout = VK_NULL_HANDLE;
    }
  }

  VkDescriptorSet createTextureDescriptorSet(VkImageView imageView, VkSampler sampler) {
    if (textureDescriptorPool == VK_NULL_HANDLE || textureDescriptorSetLayout == VK_NULL_HANDLE)
      throw RendererException("Vulkan texture descriptor resources are not initialized");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = textureDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    checkVk(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "vkAllocateDescriptorSets(texture)");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    return descriptorSet;
  }

  void destroyGraphicsPipeline() {
    if (device == VK_NULL_HANDLE)
      return;

    if (graphicsPipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, graphicsPipeline, nullptr);
      graphicsPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      pipelineLayout = VK_NULL_HANDLE;
    }
    if (vertexShaderModule != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, vertexShaderModule, nullptr);
      vertexShaderModule = VK_NULL_HANDLE;
    }
    if (fragmentShaderModule != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
      fragmentShaderModule = VK_NULL_HANDLE;
    }
  }

  void createGraphicsPipeline() {
    destroyGraphicsPipeline();

    if (renderPass == VK_NULL_HANDLE)
      return;
    if (textureDescriptorSetLayout == VK_NULL_HANDLE)
      throw RendererException("Vulkan texture descriptor set layout is not initialized");

    vertexShaderModule = createShaderModule(VulkanVertexShaderCode, sizeof(VulkanVertexShaderCode) / sizeof(uint32_t), "vkCreateShaderModule(vertex)");
    fragmentShaderModule = createShaderModule(VulkanFragmentShaderCode, sizeof(VulkanFragmentShaderCode) / sizeof(uint32_t), "vkCreateShaderModule(fragment)");

    VkPipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertexShaderModule;
    vertexStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragmentShaderModule;
    fragmentStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexStageInfo, fragmentStageInfo};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(VulkanRenderVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(VulkanRenderVertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(VulkanRenderVertex, uv);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributeDescriptions[2].offset = offsetof(VulkanRenderVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 2;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &textureDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    checkVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    checkVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &graphicsPipeline), "vkCreateGraphicsPipelines");
  }

  void ensureDynamicVertexBuffer(VkDeviceSize requiredBytes) {
    if (requiredBytes == 0)
      return;
    if (dynamicVertexBuffer.size >= requiredBytes && dynamicVertexBuffer.mapped)
      return;

    waitForQueuesIdle();
    destroyBuffer(dynamicVertexBuffer);
    dynamicVertexBufferMemoryProperties = 0;

    auto targetSize = std::max<VkDeviceSize>(requiredBytes, 1024ull * 1024ull);
    while (targetSize < requiredBytes)
      targetSize *= 2;

    constexpr VkMemoryPropertyFlags memoryCandidates[] = {
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    };

    bool allocated = false;
    for (auto memoryProperties : memoryCandidates) {
      try {
        createBuffer(targetSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            memoryProperties,
            dynamicVertexBuffer,
            "vkCreateBuffer(dynamicVertex)");
        dynamicVertexBufferMemoryProperties = memoryProperties;
        checkVk(vkMapMemory(device, dynamicVertexBuffer.memory, 0, dynamicVertexBuffer.size, 0, &dynamicVertexBuffer.mapped), "vkMapMemory(dynamicVertex)");
        allocated = true;
        break;
      } catch (std::exception const&) {
        destroyBuffer(dynamicVertexBuffer);
        dynamicVertexBufferMemoryProperties = 0;
      }
    }

    if (!allocated)
      throw RendererException("Could not allocate host-visible dynamic Vulkan vertex buffer");
  }

  void uploadFrameVertices() {
    if (frameVertices.empty())
      return;

    VkDeviceSize uploadBytes = (VkDeviceSize)frameVertices.size() * sizeof(VulkanRenderVertex);
    ensureDynamicVertexBuffer(uploadBytes);
    std::memcpy(dynamicVertexBuffer.mapped, frameVertices.data(), (size_t)uploadBytes);

    if ((dynamicVertexBufferMemoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
      VkMappedMemoryRange flushRange{};
      flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      flushRange.memory = dynamicVertexBuffer.memory;
      flushRange.offset = 0;
      flushRange.size = uploadBytes;
      checkVk(vkFlushMappedMemoryRanges(device, 1, &flushRange), "vkFlushMappedMemoryRanges(dynamicVertex)");
    }
  }

  VkCommandBuffer beginSingleUseCommands(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    checkVk(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "vkAllocateCommandBuffers(singleUse)");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(singleUse)");

    return commandBuffer;
  }

  void endSingleUseCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue) {
    checkVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(singleUse)");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    checkVk(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(singleUse)");
    checkVk(vkQueueWaitIdle(queue), "vkQueueWaitIdle(singleUse)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  }

  void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
      throw RendererException("Unsupported Vulkan texture layout transition requested");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
  }

  void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    checkVk(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage(texture)");

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory(texture)");
    checkVk(vkBindImageMemory(device, image, memory, 0), "vkBindImageMemory(texture)");
  }

  VkImageView createImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    checkVk(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "vkCreateImageView(texture)");
    return imageView;
  }

  void uploadTexturePixels(VkImage image, uint32_t width, uint32_t height, uint8_t const* pixels, size_t pixelBytes) {
    if (pixelBytes == 0)
      return;

    BufferResource stagingBuffer;
    VkMemoryPropertyFlags stagingMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer((VkDeviceSize)pixelBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingMemoryProperties, stagingBuffer, "vkCreateBuffer(textureStaging)");

    void* mapped = nullptr;
    checkVk(vkMapMemory(device, stagingBuffer.memory, 0, stagingBuffer.size, 0, &mapped), "vkMapMemory(textureStaging)");
    std::memcpy(mapped, pixels, pixelBytes);
    vkUnmapMemory(device, stagingBuffer.memory);

    auto commandPool = graphicsCommandPool;
    auto queue = graphicsQueue;
    auto commandBuffer = beginSingleUseCommands(commandPool);

    transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    endSingleUseCommands(commandBuffer, commandPool, queue);

    destroyBuffer(stagingBuffer);
  }

  void destroyTextureResources(VulkanTexture& texture) {
    if (device == VK_NULL_HANDLE)
      return;

    if (texture.descriptorSet != VK_NULL_HANDLE && textureDescriptorPool != VK_NULL_HANDLE) {
      vkFreeDescriptorSets(device, textureDescriptorPool, 1, &texture.descriptorSet);
      texture.descriptorSet = VK_NULL_HANDLE;
    }

    if (texture.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(device, texture.sampler, nullptr);
      texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.imageView != VK_NULL_HANDLE) {
      vkDestroyImageView(device, texture.imageView, nullptr);
      texture.imageView = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
      vkDestroyImage(device, texture.image, nullptr);
      texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, texture.memory, nullptr);
      texture.memory = VK_NULL_HANDLE;
    }
  }

  void destroyLiveTextures() {
    for (auto& texture : liveTextures) {
      if (texture)
        destroyTextureResources(*texture);
    }
    liveTextures.clear();
    whiteTexture.reset();
  }

  SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice deviceHandle) {
    SwapchainSupportDetails details;
    checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceHandle, surface, &details.capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t formatCount = 0;
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(deviceHandle, surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    if (formatCount > 0) {
      details.formats.resize(formatCount);
      checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(deviceHandle, surface, &formatCount, details.formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
    }

    uint32_t presentModeCount = 0;
    checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(deviceHandle, surface, &presentModeCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    if (presentModeCount > 0) {
      details.presentModes.resize(presentModeCount);
      checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(deviceHandle, surface, &presentModeCount, details.presentModes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
    }

    return details;
  }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& availableFormats) {
    for (auto const& format : availableFormats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        return format;
    }
    for (auto const& format : availableFormats) {
      if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        return format;
    }
    return availableFormats.front();
  }

  VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> const& availablePresentModes) {
    auto hasMode = [&](VkPresentModeKHR mode) {
      return std::find(availablePresentModes.begin(), availablePresentModes.end(), mode) != availablePresentModes.end();
    };

    if (hasMode(preferredPresentMode))
      return preferredPresentMode;

    for (auto presentMode : availablePresentModes) {
      if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        return presentMode;
    }
    if (hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (hasMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
      return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR const& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
      return capabilities.currentExtent;

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height))
      throw RendererException::format("SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());

    VkExtent2D extent = {
      (uint32_t)std::max(width, 0),
      (uint32_t)std::max(height, 0)
    };

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
  }

  void recreateSwapchain() {
    if (device == VK_NULL_HANDLE)
      return;

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height))
      throw RendererException::format("SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());

    if (width <= 0 || height <= 0)
      return;

    waitForFrameFences();
    waitForQueuesIdle();

    auto details = querySwapchainSupport(physicalDevice);
    if (details.formats.empty() || details.presentModes.empty())
      throw RendererException("Vulkan surface has no usable swapchain formats/present modes");

    auto surfaceFormat = chooseSwapSurfaceFormat(details.formats);
    auto presentMode = choosePresentMode(details.presentModes);
    auto extent = chooseSwapExtent(details.capabilities);

    uint32_t minimumPracticalImageCount = std::max<uint32_t>(details.capabilities.minImageCount, (uint32_t)frameSyncObjects.size() + 1);
    uint32_t imageCount = requestedSwapchainImageCount > 0 ? requestedSwapchainImageCount : minimumPracticalImageCount;
    imageCount = std::max(imageCount, minimumPracticalImageCount);
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
      imageCount = details.capabilities.maxImageCount;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (auto candidate : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
      if (details.capabilities.supportedCompositeAlpha & candidate) {
        compositeAlpha = candidate;
        break;
      }
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (details.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
      imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageUsage = imageUsage;
    uint32_t queueFamilyIndices[] = {graphicsQueueFamily, presentQueueFamily};
    if (graphicsQueueFamily != presentQueueFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;
      createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = swapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    checkVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "vkCreateSwapchainKHR");

    cleanupSwapchainObjectsExceptHandle();

    if (swapchain != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = newSwapchain;

    uint32_t swapchainImageCount = 0;
    checkVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
    swapchainImages.resize(swapchainImageCount);
    checkVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");
    imageInFlightFences.assign(swapchainImages.size(), VK_NULL_HANDLE);

    swapchainFormat = surfaceFormat.format;
    swapchainExtent = extent;

    createRenderPass();
    createGraphicsPipeline();
    createImageViews();
    createFramebuffers();
    allocateCommandBuffers();
    staticCommandBuffersReady = false;

    Logger::info("Vulkan: swapchain {}x{}, images={}, presentMode={}",
        swapchainExtent.width,
        swapchainExtent.height,
        swapchainImages.size(),
        presentModeToString(presentMode));

    swapchainDirty = false;
    recreateAfterPresent = false;
  }

  void cleanupSwapchainObjectsExceptHandle() {
    if (device == VK_NULL_HANDLE)
      return;

    destroyGraphicsPipeline();

    if (!commandBuffers.empty()) {
      vkFreeCommandBuffers(device, graphicsCommandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
      commandBuffers.clear();
    }
    staticCommandBuffersReady = false;

    for (auto framebuffer : framebuffers)
      vkDestroyFramebuffer(device, framebuffer, nullptr);
    framebuffers.clear();

    for (auto view : swapchainImageViews)
      vkDestroyImageView(device, view, nullptr);
    swapchainImageViews.clear();

    if (renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(device, renderPass, nullptr);
      renderPass = VK_NULL_HANDLE;
    }

    swapchainImages.clear();
    imageInFlightFences.clear();
  }

  void cleanupSwapchain() {
    cleanupSwapchainObjectsExceptHandle();
    if (device != VK_NULL_HANDLE && swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device, swapchain, nullptr);
      swapchain = VK_NULL_HANDLE;
    }
  }

  void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    checkVk(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "vkCreateRenderPass");
  }

  void createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapchainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapchainFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      checkVk(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]), "vkCreateImageView");
    }
  }

  void createFramebuffers() {
    framebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
      VkImageView attachments[] = {swapchainImageViews[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = swapchainExtent.width;
      framebufferInfo.height = swapchainExtent.height;
      framebufferInfo.layers = 1;

      checkVk(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]), "vkCreateFramebuffer");
    }
  }

  void allocateCommandBuffers() {
    commandBuffers.resize(framebuffers.size());
    if (commandBuffers.empty())
      return;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    checkVk(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()), "vkAllocateCommandBuffers");
    staticCommandBuffersReady = false;
  }

  void recordCommandBuffer(uint32_t imageIndex, bool oneTimeSubmit) {
    if (imageIndex >= commandBuffers.size() || imageIndex >= framebuffers.size())
      throw RendererException("Vulkan command buffer recording index out of range");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = oneTimeSubmit ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
    checkVk(vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo), "vkBeginCommandBuffer");

    VkClearValue clearColor{};
    clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!frameDraws.empty() && graphicsPipeline != VK_NULL_HANDLE && pipelineLayout != VK_NULL_HANDLE && dynamicVertexBuffer.buffer != VK_NULL_HANDLE) {
      vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

      VkViewport viewport{};
      viewport.x = 0.0f;
      viewport.y = (float)swapchainExtent.height;
      viewport.width = (float)swapchainExtent.width;
      viewport.height = -(float)swapchainExtent.height;
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);

      float screenSizePushConstants[2] = {(float)swapchainExtent.width, (float)swapchainExtent.height};
      vkCmdPushConstants(commandBuffers[imageIndex], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screenSizePushConstants), screenSizePushConstants);

      VkDeviceSize vertexOffset = 0;
      vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, &dynamicVertexBuffer.buffer, &vertexOffset);

      for (auto const& draw : frameDraws) {
        if (!draw.texture || draw.texture->descriptorSet == VK_NULL_HANDLE || draw.vertexCount == 0)
          continue;

        RectI scissorRect = draw.scissor.value(RectI::withSize(Vec2I(), Vec2I((int)swapchainExtent.width, (int)swapchainExtent.height)));
        int minX = std::clamp(scissorRect.xMin(), 0, (int)swapchainExtent.width);
        int minY = std::clamp(scissorRect.yMin(), 0, (int)swapchainExtent.height);
        int maxX = std::clamp(scissorRect.xMax(), 0, (int)swapchainExtent.width);
        int maxY = std::clamp(scissorRect.yMax(), 0, (int)swapchainExtent.height);
        if (maxX <= minX || maxY <= minY)
          continue;

        VkRect2D scissor{};
        scissor.offset = {minX, (int)swapchainExtent.height - maxY};
        scissor.extent = {(uint32_t)(maxX - minX), (uint32_t)(maxY - minY)};
        vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

        vkCmdBindDescriptorSets(commandBuffers[imageIndex],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &draw.texture->descriptorSet,
            0,
            nullptr);

        vkCmdDraw(commandBuffers[imageIndex], draw.vertexCount, 1, draw.firstVertex, 0);
      }
    }

    vkCmdEndRenderPass(commandBuffers[imageIndex]);
    checkVk(vkEndCommandBuffer(commandBuffers[imageIndex]), "vkEndCommandBuffer");
  }

  void recordStaticCommandBuffers() {
    if (commandBuffers.empty())
      return;

    for (uint32_t imageIndex = 0; imageIndex < commandBuffers.size(); ++imageIndex) {
      checkVk(vkResetCommandBuffer(commandBuffers[imageIndex], 0), "vkResetCommandBuffer(static)");
      recordCommandBuffer(imageIndex, false);
    }

    staticCommandBuffersReady = true;
    Logger::info("Vulkan: pre-recorded {} static swapchain command buffers", commandBuffers.size());
  }

  bool beginFrame() {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      swapchainDirty = true;
      frameActive = false;
      return false;
    }

    if (swapchainDirty)
      recreateSwapchain();

    if (swapchain == VK_NULL_HANDLE || framebuffers.empty() || frameSyncObjects.empty())
      return false;

    auto& frameSync = frameSyncObjects[currentFrame];
    checkVk(vkWaitForFences(device, 1, &frameSync.inFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()), "vkWaitForFences");

    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<uint64_t>::max(), frameSync.imageAvailable, VK_NULL_HANDLE, &currentImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
      swapchainDirty = true;
      recreateSwapchain();
      return false;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR)
      recreateAfterPresent = true;
    else
      checkVk(acquireResult, "vkAcquireNextImageKHR");

    if (currentImageIndex >= imageInFlightFences.size())
      throw RendererException("Vulkan frame sync state is out of range for swapchain image index");

    if (imageInFlightFences[currentImageIndex] != VK_NULL_HANDLE)
      checkVk(vkWaitForFences(device, 1, &imageInFlightFences[currentImageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max()), "vkWaitForFences(imageInFlight)");

    imageInFlightFences[currentImageIndex] = frameSync.inFlightFence;

    checkVk(vkResetFences(device, 1, &frameSync.inFlightFence), "vkResetFences");
    checkVk(vkResetCommandBuffer(commandBuffers[currentImageIndex], 0), "vkResetCommandBuffer(frame)");

    activeFrame = currentFrame;
    frameActive = true;
    return true;
  }

  void endFrame() {
    if (!frameActive || frameSyncObjects.empty())
      return;

    if (!frameDraws.empty())
      uploadFrameVertices();
    recordCommandBuffer(currentImageIndex, true);

    auto& frameSync = frameSyncObjects[activeFrame];
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentImageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.renderFinished;

    checkVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameSync.inFlightFence), "vkQueueSubmit");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameSync.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
      swapchainDirty = true;
      recreateSwapchain();
    } else {
      checkVk(presentResult, "vkQueuePresentKHR");
      if (recreateAfterPresent) {
        swapchainDirty = true;
        recreateSwapchain();
      }
    }

    currentFrame = (currentFrame + 1) % frameSyncObjects.size();
    frameActive = false;
    recreateAfterPresent = false;
  }

  SDL_Window* window = nullptr;

  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VkQueue transferQueue = VK_NULL_HANDLE;
  uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
  uint32_t presentQueueFamily = std::numeric_limits<uint32_t>::max();
  uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max();

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
  VkExtent2D swapchainExtent{0, 0};
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> framebuffers;

  VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
  VkCommandPool transferCommandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;
  VkPipelineCache pipelineCache = VK_NULL_HANDLE;
  TransferContext transferContext;

  BufferResource uploadBuffer;
  BufferResource gpuScratchBuffer;
  BufferResource dynamicVertexBuffer;
  VkMemoryPropertyFlags uploadBufferMemoryProperties = 0;
  VkMemoryPropertyFlags dynamicVertexBufferMemoryProperties = 0;

  std::vector<VulkanRenderVertex> frameVertices;
  std::vector<DrawSlice> frameDraws;
  Maybe<RectI> activeScissor;
  RefPtr<VulkanTexture> whiteTexture;
  std::vector<RefPtr<VulkanTexture>> liveTextures;

  VkDescriptorSetLayout textureDescriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
  VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;

  std::vector<FrameSync> frameSyncObjects;
  std::vector<VkFence> imageInFlightFences;

  uint32_t currentImageIndex = 0;
  size_t currentFrame = 0;
  size_t activeFrame = 0;
  bool frameActive = false;
  bool swapchainDirty = false;
  bool recreateAfterPresent = false;
  bool staticCommandBuffersReady = false;
  VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
  uint32_t framesInFlight = DefaultFramesInFlight;
  VkDeviceSize uploadBufferSize = DefaultUploadBufferSize;
  uint32_t requestedSwapchainImageCount = 0;
  bool useStaticCommandBuffers = true;
  bool enablePipelineCache = true;
  bool enableTransferQueue = true;
  String pipelineCachePath = defaultPipelineCachePath();
  bool preferDiscreteGpu = true;
  String preferredGpuName;
};

bool sameScissor(Maybe<RectI> const& a, Maybe<RectI> const& b) {
  if (!a && !b)
    return true;
  if ((bool)a != (bool)b)
    return false;
  return a.value() == b.value();
}

RefPtr<VulkanTexture> asVulkanTexture(TexturePtr const& texture, RefPtr<VulkanTexture> const& whiteTexture) {
  if (!texture)
    return whiteTexture;
  if (auto vulkanTexture = as<VulkanTexture>(texture))
    return vulkanTexture;
  return whiteTexture;
}

VulkanRenderVertex convertVertex(RenderVertex const& vertex, Mat3F const& transformation, RefPtr<VulkanTexture> const& texture) {
  Vec2F screen = transformation * vertex.screenCoordinate;
  Vec2U textureSize = texture ? texture->size() : Vec2U::filled(1);

  float invW = textureSize[0] > 0 ? 1.0f / (float)textureSize[0] : 0.0f;
  float invH = textureSize[1] > 0 ? 1.0f / (float)textureSize[1] : 0.0f;

  VulkanRenderVertex out{};
  out.pos[0] = screen[0];
  out.pos[1] = screen[1];
  out.uv[0] = vertex.textureCoordinate[0] * invW;
  out.uv[1] = vertex.textureCoordinate[1] * invH;
  out.color = vertex.color;
  return out;
}

VulkanRenderer::VulkanRenderer(void* platformWindowHandle) {
  if (!platformWindowHandle)
    throw RendererException("Vulkan renderer requires an SDL window handle");

  auto* window = static_cast<SDL_Window*>(platformWindowHandle);
  m_impl = std::make_unique<Impl>(window);

  if (char const* preferDiscreteText = std::getenv("STAR_VK_PREFER_DISCRETE")) {
    if (auto parsedPreferDiscrete = boolFromString(preferDiscreteText)) {
      m_impl->preferDiscreteGpu = *parsedPreferDiscrete;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_PREFER_DISCRETE value '{}', expected true/false", preferDiscreteText);
    }
  }

  if (char const* preferredGpuName = std::getenv("STAR_VK_GPU_NAME")) {
    auto gpuNameFilter = String(preferredGpuName).trim();
    if (!gpuNameFilter.empty()) {
      m_impl->preferredGpuName = gpuNameFilter;
      Logger::info("Vulkan: using GPU name preference filter '{}'", m_impl->preferredGpuName);
    }
  }

  if (char const* preferredMode = std::getenv("STAR_VK_PRESENT_MODE")) {
    if (auto parsedPresentMode = presentModeFromString(preferredMode)) {
      m_impl->preferredPresentMode = *parsedPresentMode;
    } else {
      Logger::warn("Vulkan: unknown STAR_VK_PRESENT_MODE '{}', using default present mode selection", preferredMode);
    }
  }

  if (char const* framesInFlightText = std::getenv("STAR_VK_FRAMES_IN_FLIGHT")) {
    if (auto parsedFramesInFlight = parseUint(framesInFlightText)) {
      m_impl->framesInFlight = *parsedFramesInFlight;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_FRAMES_IN_FLIGHT value '{}', expected integer", framesInFlightText);
    }
  }

  if (char const* uploadBufferMiBText = std::getenv("STAR_VK_UPLOAD_BUFFER_MB")) {
    if (auto parsedUploadBufferMiB = parseUint(uploadBufferMiBText)) {
      m_impl->uploadBufferSize = (VkDeviceSize)(*parsedUploadBufferMiB) * 1024ull * 1024ull;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_UPLOAD_BUFFER_MB value '{}', expected integer", uploadBufferMiBText);
    }
  }

  if (char const* swapchainImagesText = std::getenv("STAR_VK_SWAPCHAIN_IMAGES")) {
    if (auto parsedSwapchainImages = parseUint(swapchainImagesText)) {
      m_impl->requestedSwapchainImageCount = *parsedSwapchainImages;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_SWAPCHAIN_IMAGES value '{}', expected integer", swapchainImagesText);
    }
  }

  if (char const* staticCommandBuffersText = std::getenv("STAR_VK_STATIC_COMMAND_BUFFERS")) {
    if (auto parsedStaticCommandBuffers = boolFromString(staticCommandBuffersText)) {
      m_impl->useStaticCommandBuffers = *parsedStaticCommandBuffers;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_STATIC_COMMAND_BUFFERS value '{}', expected true/false", staticCommandBuffersText);
    }
  }

  if (char const* pipelineCacheEnabledText = std::getenv("STAR_VK_ENABLE_PIPELINE_CACHE")) {
    if (auto parsedPipelineCacheEnabled = boolFromString(pipelineCacheEnabledText)) {
      m_impl->enablePipelineCache = *parsedPipelineCacheEnabled;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_ENABLE_PIPELINE_CACHE value '{}', expected true/false", pipelineCacheEnabledText);
    }
  }

  if (char const* pipelineCachePathText = std::getenv("STAR_VK_PIPELINE_CACHE_PATH")) {
    auto requestedPipelineCachePath = String(pipelineCachePathText).trim();
    if (!requestedPipelineCachePath.empty())
      m_impl->pipelineCachePath = requestedPipelineCachePath;
  }

  if (char const* transferQueueEnabledText = std::getenv("STAR_VK_ENABLE_TRANSFER_QUEUE")) {
    if (auto parsedTransferQueueEnabled = boolFromString(transferQueueEnabledText)) {
      m_impl->enableTransferQueue = *parsedTransferQueueEnabled;
    } else {
      Logger::warn("Vulkan: invalid STAR_VK_ENABLE_TRANSFER_QUEUE value '{}', expected true/false", transferQueueEnabledText);
    }
  }

  m_impl->initialize();

  int width = 0;
  int height = 0;
  if (SDL_GetWindowSize(window, &width, &height))
    m_screenSize = {(unsigned)std::max(width, 0), (unsigned)std::max(height, 0)};
  else
    m_screenSize = {};

  auto whiteTexture = createTexture(Image::filled({1, 1}, Vec4B(255, 255, 255, 255), PixelFormat::RGBA32),
      TextureAddressing::Clamp,
      TextureFiltering::Nearest);
  m_impl->whiteTexture = as<VulkanTexture>(whiteTexture);
}

VulkanRenderer::~VulkanRenderer() = default;

String VulkanRenderer::rendererId() const {
  return "Vulkan11";
}

Vec2U VulkanRenderer::screenSize() const {
  return m_screenSize;
}

void VulkanRenderer::setScreenSize(Vec2U screenSize) {
  m_screenSize = screenSize;
  if (m_impl)
    m_impl->swapchainDirty = true;
}

void VulkanRenderer::loadConfig(Json const& config) {
  if (!m_impl)
    return;

  auto preferredModeName = config.getString("presentMode", "");
  if (auto preferredMode = presentModeFromString(preferredModeName)) {
    m_impl->preferredPresentMode = *preferredMode;
    m_impl->swapchainDirty = true;
  } else if (!preferredModeName.empty()) {
    Logger::warn("Vulkan renderer: unknown presentMode '{}', keeping existing present mode preference", preferredModeName);
  }

  auto requestedFramesInFlight = config.getUInt("framesInFlight", m_impl->framesInFlight);
  if (requestedFramesInFlight != m_impl->framesInFlight) {
    m_impl->framesInFlight = requestedFramesInFlight;
    m_impl->recreateSyncObjects();
    m_impl->swapchainDirty = true;
  }

  auto uploadBufferMiB = config.getUInt("uploadBufferMiB",
      config.getUInt("uploadBufferMB", (uint32_t)(m_impl->uploadBufferSize / (1024ull * 1024ull))));
  auto requestedUploadBufferSize = (VkDeviceSize)uploadBufferMiB * 1024ull * 1024ull;
  if (requestedUploadBufferSize != m_impl->uploadBufferSize) {
    m_impl->uploadBufferSize = requestedUploadBufferSize;
    m_impl->recreateUploadResources();
  }

  auto swapchainImages = config.getUInt("swapchainImages", m_impl->requestedSwapchainImageCount);
  if (swapchainImages != m_impl->requestedSwapchainImageCount) {
    m_impl->requestedSwapchainImageCount = swapchainImages;
    m_impl->swapchainDirty = true;
  }

  auto staticCommandBuffers = config.getBool("staticCommandBuffers", m_impl->useStaticCommandBuffers);
  if (staticCommandBuffers != m_impl->useStaticCommandBuffers) {
    m_impl->useStaticCommandBuffers = staticCommandBuffers;
    m_impl->staticCommandBuffersReady = false;
    m_impl->swapchainDirty = true;
  }

  auto enablePipelineCache = config.getBool("enablePipelineCache", m_impl->enablePipelineCache);
  auto requestedPipelineCachePath = config.getString("pipelineCachePath", m_impl->pipelineCachePath).trim();
  if (enablePipelineCache != m_impl->enablePipelineCache || requestedPipelineCachePath != m_impl->pipelineCachePath) {
    m_impl->waitForQueuesIdle();
    m_impl->destroyPipelineCache();

    m_impl->enablePipelineCache = enablePipelineCache;
    m_impl->pipelineCachePath = requestedPipelineCachePath;
    if (m_impl->pipelineCachePath.empty())
      m_impl->pipelineCachePath = defaultPipelineCachePath();

    m_impl->createPipelineCache();
  }

  auto requestedEnableTransferQueue = config.getBool("enableTransferQueue", m_impl->enableTransferQueue);
  if (requestedEnableTransferQueue != m_impl->enableTransferQueue) {
    m_impl->enableTransferQueue = requestedEnableTransferQueue;
    Logger::warn("Vulkan renderer: enableTransferQueue changed; restart is required to rebuild queue families");
  }

  auto requestedPreferDiscreteGpu = config.getBool("preferDiscreteGpu", m_impl->preferDiscreteGpu);
  auto requestedPreferredGpuName = config.getString("preferredGpuName", m_impl->preferredGpuName).trim();
  if (requestedPreferDiscreteGpu != m_impl->preferDiscreteGpu || requestedPreferredGpuName != m_impl->preferredGpuName) {
    m_impl->preferDiscreteGpu = requestedPreferDiscreteGpu;
    m_impl->preferredGpuName = requestedPreferredGpuName;
    Logger::warn("Vulkan renderer: GPU selection options changed; restart is required to re-pick physical device");
  }
}

void VulkanRenderer::loadEffectConfig(String const&, Json const&, StringMap<String> const&) {
  if (!m_warnedAboutEffects) {
    Logger::warn("Vulkan renderer: effect pipeline is not implemented yet; effect config calls are currently ignored");
    m_warnedAboutEffects = true;
  }
}

void VulkanRenderer::setEffectParameter(String const&, RenderEffectParameter const&) {}

void VulkanRenderer::setEffectScriptableParameter(String const&, String const&, RenderEffectParameter const&) {}

Maybe<RenderEffectParameter> VulkanRenderer::getEffectScriptableParameter(String const&, String const&) {
  return {};
}

Maybe<VariantTypeIndex> VulkanRenderer::getEffectScriptableParameterType(String const&, String const&) {
  return {};
}

void VulkanRenderer::setEffectTexture(String const&, ImageView const&) {}

bool VulkanRenderer::switchEffectConfig(String const&) {
  return false;
}

void VulkanRenderer::setScissorRect(Maybe<RectI> const& scissorRect) {
  if (!m_impl)
    return;
  m_impl->activeScissor = scissorRect;
}

TexturePtr VulkanRenderer::createTexture(Image const& texture, TextureAddressing addressing, TextureFiltering filtering) {
  if (!m_impl)
    throw RendererException("Vulkan renderer is not initialized");

  Image uploadImage = texture;
  if (uploadImage.empty())
    uploadImage = Image::filled({1, 1}, Vec4B(255, 255, 255, 255), PixelFormat::RGBA32);
  if (uploadImage.pixelFormat() != PixelFormat::RGBA32)
    uploadImage = uploadImage.convert(PixelFormat::RGBA32);

  auto vulkanTexture = make_ref<VulkanTexture>(uploadImage.size(), addressing, filtering);
  try {
    m_impl->createImage(uploadImage.width(),
        uploadImage.height(),
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        vulkanTexture->image,
        vulkanTexture->memory);

    m_impl->uploadTexturePixels(vulkanTexture->image,
        uploadImage.width(),
        uploadImage.height(),
        uploadImage.data(),
        (size_t)uploadImage.width() * uploadImage.height() * 4u);

    vulkanTexture->imageView = m_impl->createImageView(vulkanTexture->image, VK_FORMAT_R8G8B8A8_UNORM);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filtering == TextureFiltering::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerInfo.minFilter = filtering == TextureFiltering::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerInfo.addressModeU = addressing == TextureAddressing::Wrap ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = addressing == TextureAddressing::Wrap ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = addressing == TextureAddressing::Wrap ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    checkVk(vkCreateSampler(m_impl->device, &samplerInfo, nullptr, &vulkanTexture->sampler), "vkCreateSampler(texture)");

    vulkanTexture->descriptorSet = m_impl->createTextureDescriptorSet(vulkanTexture->imageView, vulkanTexture->sampler);
    m_impl->liveTextures.push_back(vulkanTexture);
    return vulkanTexture;
  } catch (...) {
    m_impl->destroyTextureResources(*vulkanTexture);
    throw;
  }
}

void VulkanRenderer::setSizeLimitEnabled(bool enabled) {
  m_limitTextureGroupSize = enabled;
}

void VulkanRenderer::setMultiTexturingEnabled(bool enabled) {
  m_useMultiTexturing = enabled;
}

void VulkanRenderer::setMultiSampling(unsigned multiSampling) {
  m_multiSampling = multiSampling;
}

TextureGroupPtr VulkanRenderer::createTextureGroup(TextureGroupSize, TextureFiltering filtering) {
  return std::make_shared<VulkanTextureGroup>(this, filtering);
}

RenderBufferPtr VulkanRenderer::createRenderBuffer() {
  return std::make_shared<VulkanRenderBuffer>();
}

List<RenderPrimitive>& VulkanRenderer::immediatePrimitives() {
  return m_immediatePrimitives;
}

void VulkanRenderer::render(RenderPrimitive primitive) {
  m_immediatePrimitives.append(std::move(primitive));
}

void VulkanRenderer::renderBuffer(RenderBufferPtr const& renderBuffer, Mat3F const& transformation) {
  if (!m_impl)
    return;

  auto appendPrimitiveList = [&](List<RenderPrimitive> const& primitives, Mat3F const& localTransform) {
    auto appendDrawVertex = [&](RefPtr<VulkanTexture> const& texture, VulkanRenderVertex const& vertex) {
      auto drawTexture = texture ? texture : m_impl->whiteTexture;
      if (!drawTexture)
        return;

      if (m_impl->frameDraws.empty()
          || m_impl->frameDraws.back().texture.get() != drawTexture.get()
          || !sameScissor(m_impl->frameDraws.back().scissor, m_impl->activeScissor)) {
        VulkanRenderer::Impl::DrawSlice drawSlice;
        drawSlice.texture = drawTexture;
        drawSlice.firstVertex = (uint32_t)m_impl->frameVertices.size();
        drawSlice.vertexCount = 0;
        drawSlice.scissor = m_impl->activeScissor;
        m_impl->frameDraws.push_back(std::move(drawSlice));
      }

      m_impl->frameVertices.push_back(vertex);
      m_impl->frameDraws.back().vertexCount += 1;
    };

    auto appendTriangle = [&](RefPtr<VulkanTexture> const& texture, RenderVertex const& a, RenderVertex const& b, RenderVertex const& c) {
      appendDrawVertex(texture, convertVertex(a, localTransform, texture));
      appendDrawVertex(texture, convertVertex(b, localTransform, texture));
      appendDrawVertex(texture, convertVertex(c, localTransform, texture));
    };

    for (auto const& primitive : primitives) {
      if (auto tri = primitive.ptr<RenderTriangle>()) {
        auto texture = asVulkanTexture(tri->texture, m_impl->whiteTexture);
        appendTriangle(texture, tri->a, tri->b, tri->c);
      } else if (auto quad = primitive.ptr<RenderQuad>()) {
        auto texture = asVulkanTexture(quad->texture, m_impl->whiteTexture);
        appendTriangle(texture, quad->a, quad->b, quad->c);
        appendTriangle(texture, quad->a, quad->c, quad->d);
      } else if (auto poly = primitive.ptr<RenderPoly>()) {
        if (poly->vertexes.size() < 3)
          continue;
        auto texture = asVulkanTexture(poly->texture, m_impl->whiteTexture);
        for (size_t i = 1; i + 1 < poly->vertexes.size(); ++i)
          appendTriangle(texture, poly->vertexes[0], poly->vertexes[i], poly->vertexes[i + 1]);
      }
    }
  };

  if (!m_immediatePrimitives.empty()) {
    appendPrimitiveList(m_immediatePrimitives, Mat3F::identity());
    m_immediatePrimitives.clear();
  }

  auto vulkanBuffer = convert<VulkanRenderBuffer>(renderBuffer);
  appendPrimitiveList(vulkanBuffer->primitives(), transformation);
}

void VulkanRenderer::flush(Mat3F const& transformation) {
  if (!m_impl)
    return;
  if (m_immediatePrimitives.empty())
    return;

  auto appendDrawVertex = [&](RefPtr<VulkanTexture> const& texture, VulkanRenderVertex const& vertex) {
    auto drawTexture = texture ? texture : m_impl->whiteTexture;
    if (!drawTexture)
      return;

    if (m_impl->frameDraws.empty()
        || m_impl->frameDraws.back().texture.get() != drawTexture.get()
        || !sameScissor(m_impl->frameDraws.back().scissor, m_impl->activeScissor)) {
      VulkanRenderer::Impl::DrawSlice drawSlice;
      drawSlice.texture = drawTexture;
      drawSlice.firstVertex = (uint32_t)m_impl->frameVertices.size();
      drawSlice.vertexCount = 0;
      drawSlice.scissor = m_impl->activeScissor;
      m_impl->frameDraws.push_back(std::move(drawSlice));
    }

    m_impl->frameVertices.push_back(vertex);
    m_impl->frameDraws.back().vertexCount += 1;
  };

  auto appendTriangle = [&](RefPtr<VulkanTexture> const& texture, RenderVertex const& a, RenderVertex const& b, RenderVertex const& c) {
    appendDrawVertex(texture, convertVertex(a, transformation, texture));
    appendDrawVertex(texture, convertVertex(b, transformation, texture));
    appendDrawVertex(texture, convertVertex(c, transformation, texture));
  };

  for (auto const& primitive : m_immediatePrimitives) {
    if (auto tri = primitive.ptr<RenderTriangle>()) {
      auto texture = asVulkanTexture(tri->texture, m_impl->whiteTexture);
      appendTriangle(texture, tri->a, tri->b, tri->c);
    } else if (auto quad = primitive.ptr<RenderQuad>()) {
      auto texture = asVulkanTexture(quad->texture, m_impl->whiteTexture);
      appendTriangle(texture, quad->a, quad->b, quad->c);
      appendTriangle(texture, quad->a, quad->c, quad->d);
    } else if (auto poly = primitive.ptr<RenderPoly>()) {
      if (poly->vertexes.size() < 3)
        continue;
      auto texture = asVulkanTexture(poly->texture, m_impl->whiteTexture);
      for (size_t i = 1; i + 1 < poly->vertexes.size(); ++i)
        appendTriangle(texture, poly->vertexes[0], poly->vertexes[i], poly->vertexes[i + 1]);
    }
  }

  m_immediatePrimitives.clear();
}

void VulkanRenderer::startFrame() {
  if (!m_impl)
    return;
  if (m_impl->beginFrame()) {
    m_impl->frameVertices.clear();
    m_impl->frameDraws.clear();
  }
}

void VulkanRenderer::finishFrame() {
  if (!m_impl)
    return;

  flush(Mat3F::identity());
  m_impl->endFrame();
  m_impl->frameVertices.clear();
  m_impl->frameDraws.clear();
}

}
