#pragma once

#include <memory>

#include "StarRenderer.hpp"

namespace Star {

class VulkanRenderer : public Renderer {
public:
  explicit VulkanRenderer(void* platformWindowHandle);
  ~VulkanRenderer();

  String rendererId() const override;
  Vec2U screenSize() const override;
  void setScreenSize(Vec2U screenSize) override;

  void loadConfig(Json const& config) override;
  void loadEffectConfig(String const& name, Json const& effectConfig, StringMap<String> const& shaders) override;

  void setEffectParameter(String const& parameterName, RenderEffectParameter const& parameter) override;
  void setEffectScriptableParameter(String const& effectName, String const& parameterName, RenderEffectParameter const& parameter) override;
  Maybe<RenderEffectParameter> getEffectScriptableParameter(String const& effectName, String const& parameterName) override;
  Maybe<VariantTypeIndex> getEffectScriptableParameterType(String const& effectName, String const& parameterName) override;
  void setEffectTexture(String const& textureName, ImageView const& image) override;
  bool switchEffectConfig(String const& name) override;

  void setScissorRect(Maybe<RectI> const& scissorRect) override;

  TexturePtr createTexture(Image const& texture, TextureAddressing addressing, TextureFiltering filtering) override;
  void setSizeLimitEnabled(bool enabled) override;
  void setMultiTexturingEnabled(bool enabled) override;
  void setMultiSampling(unsigned multiSampling) override;
  TextureGroupPtr createTextureGroup(TextureGroupSize size, TextureFiltering filtering) override;
  RenderBufferPtr createRenderBuffer() override;

  List<RenderPrimitive>& immediatePrimitives() override;
  void render(RenderPrimitive primitive) override;
  void renderBuffer(RenderBufferPtr const& renderBuffer, Mat3F const& transformation) override;

  void flush(Mat3F const& transformation) override;

  void startFrame() override;
  void finishFrame() override;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  Vec2U m_screenSize;
  List<RenderPrimitive> m_immediatePrimitives;

  bool m_warnedAboutEffects = false;
  bool m_warnedAboutScissor = false;

  // Partial Vulkan effect compatibility: world light-map parameters are
  // applied during CPU-side vertex packing.
  bool m_lightMapEnabled = false;
  float m_lightMapMultiplier = 1.0f;
  Vec2F m_lightMapScale = Vec2F::filled(1.0f);
  Vec2F m_lightMapOffset{};
  Image m_lightMapImage;
  bool m_hasLightMapImage = false;

  bool m_limitTextureGroupSize = false;
  bool m_useMultiTexturing = true;
  unsigned m_multiSampling = 0;
};

}
