#ifndef D3WASM_GLSL_SHADERS_H
#define D3WASM_GLSL_SHADERS_H

// Main Light Interaction
  // gouraud
extern const char* const interactionShaderVP;
extern const char* const interactionShaderFP;
  // phong
extern const char* const interactionPhongShaderVP;
extern const char* const interactionPhongShaderFP;
// Fog
extern const char* const fogShaderVP;
extern const char* const fogShaderFP;
// Depth Buffer
  // no clip planes
extern const char* const zfillShaderVP;
extern const char* const zfillShaderFP;
  // clip planes
extern const char* const zfillClipShaderVP;
extern const char* const zfillClipShaderFP;
// Ambient Surfaces
  // diffuse mapping (default diffuse surfaces)
extern const char* const diffuseMapShaderVP;
extern const char* const diffuseMapShaderFP;
  // cube mapping (skybox/wobblesky, diffusecube, reflection)
extern const char* const diffuseCubeShaderVP;
extern const char* const skyboxCubeShaderVP;
extern const char* const reflectionCubeShaderVP;
extern const char* const cubeMapShaderFP;
// Shadows
extern const char* const stencilShadowShaderVP;
extern const char* const stencilShadowShaderFP;

#endif //D3WASM_GLSL_SHADERS_H
