#include "StarRendererFactory.hpp"

#include "StarLogging.hpp"

#ifdef STAR_ENABLE_OPENGL_RENDERER
#include "StarRenderer_opengl.hpp"
#endif

#ifdef STAR_ENABLE_VULKAN_RENDERER
#include "StarRenderer_vulkan.hpp"
#endif

namespace Star {

EnumMap<RendererBackend> const RendererBackendNames{
  {RendererBackend::Auto, "auto"},
  {RendererBackend::OpenGL, "opengl"},
  {RendererBackend::Vulkan, "vulkan"},
  {RendererBackend::Direct3D12, "direct3d12"},
  {RendererBackend::Metal, "metal"}
};

RendererBackend rendererBackendFromString(String const& backendName, RendererBackend defaultBackend) {
  auto normalizedName = backendName.trim().toLower();
  if (normalizedName.empty())
    return defaultBackend;

  if (auto backend = RendererBackendNames.maybeLeft(normalizedName))
    return *backend;

  if (normalizedName == "gl")
    return RendererBackend::OpenGL;
  if (normalizedName == "dx12" || normalizedName == "d3d12")
    return RendererBackend::Direct3D12;

  return defaultBackend;
}

RendererBackend resolveRendererBackend(RendererBackend requestedBackend) {
  if (requestedBackend == RendererBackend::Auto) {
#ifdef STAR_ENABLE_VULKAN_RENDERER
    return RendererBackend::Vulkan;
#elif defined(STAR_ENABLE_OPENGL_RENDERER)
    return RendererBackend::OpenGL;
#endif
  }

#ifdef STAR_ENABLE_OPENGL_RENDERER
  if (requestedBackend == RendererBackend::OpenGL)
    return RendererBackend::OpenGL;
#endif

#ifdef STAR_ENABLE_VULKAN_RENDERER
  if (requestedBackend == RendererBackend::Vulkan)
    return RendererBackend::Vulkan;
#endif

  RendererBackend fallbackBackend = RendererBackend::Auto;
#ifdef STAR_ENABLE_VULKAN_RENDERER
  fallbackBackend = RendererBackend::Vulkan;
#elif defined(STAR_ENABLE_OPENGL_RENDERER)
  fallbackBackend = RendererBackend::OpenGL;
#else
  fallbackBackend = RendererBackend::Auto;
#endif

  if (fallbackBackend == RendererBackend::Auto)
    throw RendererException("No renderer backends are enabled in this build");

  Logger::warn("Requested renderer backend '{}' is not available in this build, falling back to '{}'",
      RendererBackendNames.getRight(requestedBackend),
      RendererBackendNames.getRight(fallbackBackend));
  return fallbackBackend;
}

bool rendererBackendUsesOpenGlContext(RendererBackend backend) {
#ifdef STAR_ENABLE_OPENGL_RENDERER
  return backend == RendererBackend::OpenGL;
#else
  return false;
#endif
}

bool rendererBackendUsesVulkanSurface(RendererBackend backend) {
#ifdef STAR_ENABLE_VULKAN_RENDERER
  return backend == RendererBackend::Vulkan;
#else
  return false;
#endif
}

RendererPtr createRenderer(RendererBackend backend, void* platformWindowHandle) {
  switch (backend) {
  case RendererBackend::OpenGL:
#ifdef STAR_ENABLE_OPENGL_RENDERER
    return make_shared<OpenGlRenderer>();
#endif
    break;
  case RendererBackend::Vulkan:
#ifdef STAR_ENABLE_VULKAN_RENDERER
    return make_shared<VulkanRenderer>(platformWindowHandle);
#endif
    break;
  case RendererBackend::Auto:
  case RendererBackend::Direct3D12:
  case RendererBackend::Metal:
    break;
  }

  throw RendererException::format("Renderer backend '{}' cannot be created in this build",
      RendererBackendNames.getRight(backend));
}

}
