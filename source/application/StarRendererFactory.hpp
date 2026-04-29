#pragma once

#include "StarRenderer.hpp"

namespace Star {

enum class RendererBackend {
  Auto,
  OpenGL,
  Vulkan,
  Direct3D12,
  Metal
};

extern EnumMap<RendererBackend> const RendererBackendNames;

// Parse a renderer backend name. Unknown names return defaultBackend.
RendererBackend rendererBackendFromString(String const& backendName, RendererBackend defaultBackend = RendererBackend::Auto);

// Resolve to a backend available in this build.
// Auto currently prefers Vulkan, then OpenGL.
RendererBackend resolveRendererBackend(RendererBackend requestedBackend);

// Whether this backend requires an SDL OpenGL context.
bool rendererBackendUsesOpenGlContext(RendererBackend backend);
// Whether this backend requires an SDL Vulkan-capable window.
bool rendererBackendUsesVulkanSurface(RendererBackend backend);

// Construct the renderer for the given resolved backend.
RendererPtr createRenderer(RendererBackend backend, void* platformWindowHandle = nullptr);

}
