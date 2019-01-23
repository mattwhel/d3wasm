/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "sys/platform.h"
#include "renderer/VertexCache.h"

#include "tr_local.h"

#include "glsl/glsl_shaders.h"

shaderProgram_t interactionShader;
shaderProgram_t interactionPhongShader;
shaderProgram_t fogShader;
shaderProgram_t zfillShader;
shaderProgram_t zfillClipShader;
shaderProgram_t defaultSurfaceShader;
shaderProgram_t diffuseCubeShader;
shaderProgram_t skyboxCubeShader;
shaderProgram_t reflectionCubeShader;
shaderProgram_t stencilShadowShader;

/*
====================
GL_UseProgram
====================
*/
void GL_UseProgram(shaderProgram_t* program) {
  if ( backEnd.glState.currentProgram == program ) {
    return;
  }

  qglUseProgram(program ? program->program : 0);
  backEnd.glState.currentProgram = program;
}

/*
====================
GL_Uniform1fv
====================
*/
static void GL_Uniform1fv(GLint location, const GLfloat* value) {
  qglUniform1fv(*( GLint * )((char*) backEnd.glState.currentProgram + location), 1, value);
}

/*
====================
GL_Uniform1iv
====================
*/
static void GL_Uniform1iv(GLint location, const GLint* value) {
  qglUniform1iv(*( GLint * )((char*) backEnd.glState.currentProgram + location), 1, value);
}

/*
====================
GL_Uniform4fv
====================
*/
static void GL_Uniform4fv(GLint location, const GLfloat* value) {
  qglUniform4fv(*( GLint * )((char*) backEnd.glState.currentProgram + location), 1, value);
}

/*
====================
GL_UniformMatrix4fv
====================
*/
static void GL_UniformMatrix4fv(GLint location, const GLfloat* value) {
  qglUniformMatrix4fv(*( GLint * )((char*) backEnd.glState.currentProgram + location), 1, GL_FALSE, value);
}

/*
====================
GL_EnableVertexAttribArray
====================
*/
void GL_EnableVertexAttribArray(GLuint index) {
  qglEnableVertexAttribArray(*( GLint * )((char*) backEnd.glState.currentProgram + index));
}

/*
====================
GL_DisableVertexAttribArray
====================
*/
void GL_DisableVertexAttribArray(GLuint index) {
  qglDisableVertexAttribArray(*( GLint * )((char*) backEnd.glState.currentProgram + index));
}

/*
====================
GL_VertexAttribPointer
====================
*/
static void GL_VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                   GLboolean normalized, GLsizei stride,
                                   const GLvoid* pointer) {
  qglVertexAttribPointer(*( GLint * )((char*) backEnd.glState.currentProgram + index),
                         size, type, normalized, stride, pointer);
}

/*
=================
R_LoadGLSLShader

loads GLSL vertex or fragment shaders
=================
*/
static void R_LoadGLSLShader(const char* buffer, shaderProgram_t* shaderProgram, GLenum type) {
  if ( !glConfig.isInitialized ) {
    return;
  }

  switch ( type ) {
    case GL_VERTEX_SHADER:
      // create vertex shader
      shaderProgram->vertexShader = qglCreateShader(GL_VERTEX_SHADER);
      qglShaderSource(shaderProgram->vertexShader, 1, (const GLchar**) &buffer, 0);
      qglCompileShader(shaderProgram->vertexShader);
      break;
    case GL_FRAGMENT_SHADER:
      // create fragment shader
      shaderProgram->fragmentShader = qglCreateShader(GL_FRAGMENT_SHADER);
      qglShaderSource(shaderProgram->fragmentShader, 1, (const GLchar**) &buffer, 0);
      qglCompileShader(shaderProgram->fragmentShader);
      break;
    default:
      common->Printf("R_LoadGLSLShader: no type\n");
      return;
  }
}

/*
=================
R_LinkGLSLShader

links the GLSL vertex and fragment shaders together to form a GLSL program
=================
*/
static bool R_LinkGLSLShader(shaderProgram_t* shaderProgram, const char* name) {
  char buf[BUFSIZ];
  int len;
  GLint linked;

  shaderProgram->program = qglCreateProgram();

  qglAttachShader(shaderProgram->program, shaderProgram->vertexShader);
  qglAttachShader(shaderProgram->program, shaderProgram->fragmentShader);

  // Always prebind attribute 0, which is a mandatory requirement for WebGL
  // Let the rest be decided by GL
  qglBindAttribLocation(shaderProgram->program, 0, "attr_Vertex");

  qglLinkProgram(shaderProgram->program);

  qglGetProgramiv(shaderProgram->program, GL_LINK_STATUS, &linked);

  if ( com_developer.GetBool()) {
    qglGetShaderInfoLog(shaderProgram->vertexShader, sizeof(buf), &len, buf);
    common->Printf("VS:\n%.*s\n", len, buf);
    qglGetShaderInfoLog(shaderProgram->fragmentShader, sizeof(buf), &len, buf);
    common->Printf("FS:\n%.*s\n", len, buf);
  }

  if ( !linked ) {
    common->Error("R_LinkGLSLShader: program failed to link: %s\n", name);
    return false;
  }

  return true;
}

/*
=================
R_ValidateGLSLProgram

makes sure GLSL program is valid
=================
*/
static bool R_ValidateGLSLProgram(shaderProgram_t* shaderProgram) {
  GLint validProgram;

  qglValidateProgram(shaderProgram->program);

  qglGetProgramiv(shaderProgram->program, GL_VALIDATE_STATUS, &validProgram);

  if ( !validProgram ) {
    common->Printf("R_ValidateGLSLProgram: program invalid\n");
    return false;
  }

  return true;
}

/*
=================
RB_GLSL_GetUniformLocations

=================
*/
static void RB_GLSL_GetUniformLocations(shaderProgram_t* shader) {
  int i;
  char buffer[32];

  GL_UseProgram(shader);

  shader->localLightOrigin = qglGetUniformLocation(shader->program, "u_lightOrigin");
  shader->localViewOrigin = qglGetUniformLocation(shader->program, "u_viewOrigin");
  shader->lightProjectionS = qglGetUniformLocation(shader->program, "u_lightProjectionS");
  shader->lightProjectionT = qglGetUniformLocation(shader->program, "u_lightProjectionT");
  shader->lightProjectionQ = qglGetUniformLocation(shader->program, "u_lightProjectionQ");
  shader->lightFalloff = qglGetUniformLocation(shader->program, "u_lightFalloff");
  shader->bumpMatrixS = qglGetUniformLocation(shader->program, "u_bumpMatrixS");
  shader->bumpMatrixT = qglGetUniformLocation(shader->program, "u_bumpMatrixT");
  shader->diffuseMatrixS = qglGetUniformLocation(shader->program, "u_diffuseMatrixS");
  shader->diffuseMatrixT = qglGetUniformLocation(shader->program, "u_diffuseMatrixT");
  shader->specularMatrixS = qglGetUniformLocation(shader->program, "u_specularMatrixS");
  shader->specularMatrixT = qglGetUniformLocation(shader->program, "u_specularMatrixT");
  shader->colorModulate = qglGetUniformLocation(shader->program, "u_colorModulate");
  shader->colorAdd = qglGetUniformLocation(shader->program, "u_colorAdd");
  shader->fogColor = qglGetUniformLocation(shader->program, "u_fogColor");
  shader->diffuseColor = qglGetUniformLocation(shader->program, "u_diffuseColor");
  shader->specularColor = qglGetUniformLocation(shader->program, "u_specularColor");
  shader->glColor = qglGetUniformLocation(shader->program, "u_glColor");
  shader->alphaTest = qglGetUniformLocation(shader->program, "u_alphaTest");
  shader->specularExponent = qglGetUniformLocation(shader->program, "u_specularExponent");
  shader->modelViewProjectionMatrix = qglGetUniformLocation(shader->program, "u_modelViewProjectionMatrix");
  shader->modelViewMatrix = qglGetUniformLocation(shader->program, "u_modelViewMatrix");
  shader->textureMatrix = qglGetUniformLocation(shader->program, "u_textureMatrix");
  shader->texGen0S = qglGetUniformLocation(shader->program, "u_texGen0S");
  shader->texGen0T = qglGetUniformLocation(shader->program, "u_texGen0T");
  shader->texGen1S = qglGetUniformLocation(shader->program, "u_texGen1S");
  shader->texGen1T = qglGetUniformLocation(shader->program, "u_texGen1T");

  for ( i = 0; i < MAX_FRAGMENT_IMAGES; i++ ) {
    idStr::snPrintf(buffer, sizeof(buffer), "u_fragmentMap%d", i);
    shader->u_fragmentMap[i] = qglGetUniformLocation(shader->program, buffer);
    qglUniform1i(shader->u_fragmentMap[i], i);
  }

  for ( i = 0; i < MAX_FRAGMENT_IMAGES; i++ ) {
    idStr::snPrintf(buffer, sizeof(buffer), "u_fragmentCubeMap%d", i);
    shader->u_fragmentCubeMap[i] = qglGetUniformLocation(shader->program, buffer);
    qglUniform1i(shader->u_fragmentCubeMap[i], i);
  }

  shader->attr_TexCoord = qglGetAttribLocation(shader->program, "attr_TexCoord");
  shader->attr_Tangent = qglGetAttribLocation(shader->program, "attr_Tangent");
  shader->attr_Bitangent = qglGetAttribLocation(shader->program, "attr_Bitangent");
  shader->attr_Normal = qglGetAttribLocation(shader->program, "attr_Normal");
  shader->attr_Vertex = qglGetAttribLocation(shader->program, "attr_Vertex");
  shader->attr_Color = qglGetAttribLocation(shader->program, "attr_Color");

  // Always enable the vertex attribute array
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Vertex));

  GL_CheckErrors();

  GL_UseProgram(NULL);
}

/*
=================
RB_GLSL_InitShaders

=================
*/
static bool RB_GLSL_InitShaders(void) {
  // main Interaction shader
  common->Printf("Loading main interaction shader\n");
  memset(&interactionShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(interactionShaderVP, &interactionShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(interactionShaderFP, &interactionShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&interactionShader, "interaction") && !R_ValidateGLSLProgram(&interactionShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&interactionShader);
  }

  // main Interaction shader, Phong version
  common->Printf("Loading main interaction shader (Phong) \n");
  memset(&interactionPhongShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(interactionPhongShaderVP, &interactionPhongShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(interactionPhongShaderFP, &interactionPhongShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&interactionPhongShader, "interactionPhong") &&
       !R_ValidateGLSLProgram(&interactionPhongShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&interactionPhongShader);
  }

  // default surface shader
  common->Printf("Loading default surface shader\n");
  memset(&defaultSurfaceShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(defaultSurfaceShaderVP, &defaultSurfaceShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(diffuseMapShaderFP, &defaultSurfaceShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&defaultSurfaceShader, "defaultSurface") && !R_ValidateGLSLProgram(&defaultSurfaceShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&defaultSurfaceShader);
  }

  // Skybox cubemap shader
  common->Printf("Loading skybox cubemap shader\n");
  memset(&skyboxCubeShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(skyboxCubeShaderVP, &skyboxCubeShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(cubeMapShaderFP, &skyboxCubeShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&skyboxCubeShader, "skyboxCubeShader") && !R_ValidateGLSLProgram(&skyboxCubeShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&skyboxCubeShader);
  }

  // Reflection cubemap shader
  common->Printf("Loading reflection cubemap shader\n");
  memset(&reflectionCubeShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(reflectionCubeShaderVP, &reflectionCubeShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(cubeMapShaderFP, &reflectionCubeShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&reflectionCubeShader, "reflectionCubeShader") &&
       !R_ValidateGLSLProgram(&reflectionCubeShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&reflectionCubeShader);
  }

  // Diffuse cubemap shader
  common->Printf("Loading diffuse cubemap shader\n");
  memset(&diffuseCubeShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(diffuseCubeShaderVP, &diffuseCubeShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(cubeMapShaderFP, &diffuseCubeShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&diffuseCubeShader, "diffuseCubeShader") && !R_ValidateGLSLProgram(&diffuseCubeShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&diffuseCubeShader);
  }

  // Z Fill shader
  common->Printf("Loading Zfill shader\n");
  memset(&zfillShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(zfillShaderVP, &zfillShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(zfillShaderFP, &zfillShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&zfillShader, "zfill") && !R_ValidateGLSLProgram(&zfillShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&zfillShader);
  }

  // Z Fill shader, Clip planes version
  common->Printf("Loading Zfill shader (Clip plane version)\n");
  memset(&zfillClipShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(zfillClipShaderVP, &zfillClipShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(zfillClipShaderFP, &zfillClipShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&zfillClipShader, "zfillclip") && !R_ValidateGLSLProgram(&zfillClipShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&zfillClipShader);
  }

  // Fog shader
  common->Printf("Loading Fog shader\n");
  memset(&fogShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(fogShaderVP, &fogShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(fogShaderFP, &fogShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&fogShader, "fog") && !R_ValidateGLSLProgram(&fogShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&fogShader);
  }

  common->Printf("Loading BlendLight shader\n");

  // Stencil shadow shader
  common->Printf("Loading Stencil shadow shader\n");
  memset(&stencilShadowShader, 0, sizeof(shaderProgram_t));

  R_LoadGLSLShader(stencilShadowShaderVP, &stencilShadowShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(stencilShadowShaderFP, &stencilShadowShader, GL_FRAGMENT_SHADER);

  if ( !R_LinkGLSLShader(&stencilShadowShader, "shadow") && !R_ValidateGLSLProgram(&stencilShadowShader)) {
    return false;
  }
  else {
    RB_GLSL_GetUniformLocations(&stencilShadowShader);
  }

  return true;
}

/*
==================
R_ReloadGLSLPrograms_f
==================
*/
void R_ReloadGLSLPrograms_f(const idCmdArgs& args) {
  common->Printf("----- R_ReloadGLSLPrograms -----\n");

  if ( !RB_GLSL_InitShaders()) {
    common->Printf("GLSL shaders failed to init.\n");
  }

  common->Printf("-------------------------------\n");
}

/*
===============
RB_EnterWeaponDepthHack
===============
*/
static void RB_GLSL_EnterWeaponDepthHack(const drawSurf_t* surf) {
  qglDepthRangef(0, 0.5);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] *= 0.25;

  float mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, matrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
}

/*
===============
RB_EnterModelDepthHack
===============
*/
static void RB_GLSL_EnterModelDepthHack(const drawSurf_t* surf) {
  qglDepthRangef(0.0f, 1.0f);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] -= surf->space->modelDepthHack;

  float mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, matrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
}

/*
===============
RB_LeaveDepthHack
===============
*/
static void RB_GLSL_LeaveDepthHack(const drawSurf_t* surf) {
  qglDepthRangef(0, 1);

  float mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
}

/*
==================
RB_GLSL_DrawInteraction
==================
*/
static void RB_GLSL_DrawInteraction(const drawInteraction_t* din) {
  static const float zero[4] = { 0, 0, 0, 0 };
  static const float one[4] = { 1, 1, 1, 1 };
  static const float negOne[4] = { -1, -1, -1, -1 };

  // load all the vertex program parameters
  GL_Uniform4fv(offsetof(shaderProgram_t, localLightOrigin), din->localLightOrigin.ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, localViewOrigin), din->localViewOrigin.ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, lightProjectionS), din->lightProjection[0].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, lightProjectionT), din->lightProjection[1].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, lightProjectionQ), din->lightProjection[2].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, lightFalloff), din->lightProjection[3].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, bumpMatrixS), din->bumpMatrix[0].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, bumpMatrixT), din->bumpMatrix[1].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, diffuseMatrixS), din->diffuseMatrix[0].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, diffuseMatrixT), din->diffuseMatrix[1].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, specularMatrixS), din->specularMatrix[0].ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, specularMatrixT), din->specularMatrix[1].ToFloatPtr());

  switch ( din->vertexColor ) {
    case SVC_MODULATE:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), one);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), zero);
      break;
    case SVC_INVERSE_MODULATE:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), negOne);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
      break;
    case SVC_IGNORE:
    default:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), zero);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
      break;
  }

  // set the constant colors
  GL_Uniform4fv(offsetof(shaderProgram_t, diffuseColor), din->diffuseColor.ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, specularColor), din->specularColor.ToFloatPtr());

  // material may be NULL for shadow volumes
  if ( r_usePhong.GetBool()) {
    float f;
    switch ( din->surf->material->GetSurfaceType()) {
      case SURFTYPE_METAL:
      case SURFTYPE_RICOCHET:
        f = r_specularExponent.GetFloat();  // Maybe this should be adjusted
        break;
      case SURFTYPE_STONE:
      case SURFTYPE_FLESH:
      case SURFTYPE_WOOD:
      case SURFTYPE_CARDBOARD:
      case SURFTYPE_LIQUID:
      case SURFTYPE_GLASS:
      case SURFTYPE_PLASTIC:
      case SURFTYPE_NONE:
      default:
        f = r_specularExponent.GetFloat();
        break;
    }
    GL_Uniform1fv(offsetof(shaderProgram_t, specularExponent), &f);

  }

  // set the textures

  // texture 0 will be the per-surface bump map
  GL_SelectTexture(0);
  din->bumpImage->Bind();

  // texture 1 will be the light falloff texture
  GL_SelectTexture(1);
  din->lightFalloffImage->Bind();

  // texture 2 will be the light projection texture
  GL_SelectTexture(2);
  din->lightImage->Bind();

  // texture 3 is the per-surface diffuse map
  GL_SelectTexture(3);
  din->diffuseImage->Bind();

  // texture 4 is the per-surface specular map
  GL_SelectTexture(4);
  din->specularImage->Bind();

  // draw it
  RB_DrawElementsWithCounters(din->surf->geo);
}

/*
=============
RB_CreateSingleDrawInteractions

This can be used by different draw_* backends to decompose a complex light / surface
interaction into primitive interactions
=============
*/
static void
RB_GLSL_CreateSingleDrawInteractions(const drawSurf_t* surf, void (* DrawInteraction)(const drawInteraction_t*)) {
  const idMaterial* surfaceShader = surf->material;
  const float* surfaceRegs = surf->shaderRegisters;
  const viewLight_t* vLight = backEnd.vLight;
  const idMaterial* lightShader = vLight->lightShader;
  const float* lightRegs = vLight->shaderRegisters;
  drawInteraction_t inter;

  if ( r_skipInteractions.GetBool() || !surf->geo || !surf->geo->ambientCache ) {
    return;
  }

  // change the scissor if needed
  if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals(surf->scissorRect)) {
    backEnd.currentScissor = surf->scissorRect;
    if (( backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1 ) < 0.0f ||
        ( backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 ) < 0.0f ) {
      backEnd.currentScissor = backEnd.viewDef->scissor;
    }
    qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
               backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
               backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
               backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
  }

  // hack depth range if needed
  if ( surf->space->weaponDepthHack ) {
    RB_GLSL_EnterWeaponDepthHack(surf);
  }

  if ( surf->space->modelDepthHack != 0 ) {
    RB_GLSL_EnterModelDepthHack(surf);
  }

  inter.surf = surf;
  inter.lightFalloffImage = vLight->falloffImage;

  R_GlobalPointToLocal(surf->space->modelMatrix, vLight->globalLightOrigin, inter.localLightOrigin.ToVec3());
  R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, inter.localViewOrigin.ToVec3());
  inter.localLightOrigin[3] = 0;
  inter.localViewOrigin[3] = 1;
  inter.ambientLight = lightShader->IsAmbientLight();

  // the base projections may be modified by texture matrix on light stages
  idPlane lightProject[4];

  for ( int i = 0; i < 4; i++ ) {
    R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i]);
  }

  for ( int lightStageNum = 0; lightStageNum < lightShader->GetNumStages(); lightStageNum++ ) {
    const shaderStage_t* lightStage = lightShader->GetStage(lightStageNum);

    // ignore stages that fail the condition
    if ( !lightRegs[lightStage->conditionRegister] ) {
      continue;
    }

    inter.lightImage = lightStage->texture.image;

    memcpy(inter.lightProjection, lightProject, sizeof(inter.lightProjection));

    // now multiply the texgen by the light texture matrix
    if ( lightStage->texture.hasMatrix ) {
      RB_GetShaderTextureMatrix(lightRegs, &lightStage->texture, backEnd.lightTextureMatrix);
      RB_BakeTextureMatrixIntoTexgen(reinterpret_cast<class idPlane*>(inter.lightProjection), NULL);
    }

    inter.bumpImage = NULL;
    inter.specularImage = NULL;
    inter.diffuseImage = NULL;
    inter.diffuseColor[0] = inter.diffuseColor[1] = inter.diffuseColor[2] = inter.diffuseColor[3] = 0;
    inter.specularColor[0] = inter.specularColor[1] = inter.specularColor[2] = inter.specularColor[3] = 0;

    float lightColor[4];

    const float lightscale = r_lightScale.GetFloat();
    lightColor[0] = lightscale * lightRegs[lightStage->color.registers[0]];
    lightColor[1] = lightscale * lightRegs[lightStage->color.registers[1]];
    lightColor[2] = lightscale * lightRegs[lightStage->color.registers[2]];
    lightColor[3] = lightRegs[lightStage->color.registers[3]];

    // go through the individual stages
    for ( int surfaceStageNum = 0; surfaceStageNum < surfaceShader->GetNumStages(); surfaceStageNum++ ) {
      const shaderStage_t* surfaceStage = surfaceShader->GetStage(surfaceStageNum);

      switch ( surfaceStage->lighting ) {
        case SL_AMBIENT: {
          // ignore ambient stages while drawing interactions
          break;
        }
        case SL_BUMP: {
          // ignore stage that fails the condition
          if ( !surfaceRegs[surfaceStage->conditionRegister] ) {
            break;
          }

          // draw any previous interaction
          RB_SubmittInteraction(&inter, DrawInteraction);
          inter.diffuseImage = NULL;
          inter.specularImage = NULL;
          RB_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.bumpImage, inter.bumpMatrix, NULL);
          break;
        }
        case SL_DIFFUSE: {
          // ignore stage that fails the condition
          if ( !surfaceRegs[surfaceStage->conditionRegister] ) {
            break;
          }

          if ( inter.diffuseImage ) {
            RB_SubmittInteraction(&inter, DrawInteraction);
          }

          RB_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.diffuseImage,
                                inter.diffuseMatrix, inter.diffuseColor.ToFloatPtr());
          inter.diffuseColor[0] *= lightColor[0];
          inter.diffuseColor[1] *= lightColor[1];
          inter.diffuseColor[2] *= lightColor[2];
          inter.diffuseColor[3] *= lightColor[3];
          inter.vertexColor = surfaceStage->vertexColor;
          break;
        }
        case SL_SPECULAR: {
          // ignore stage that fails the condition
          if ( !surfaceRegs[surfaceStage->conditionRegister] ) {
            break;
          }

          if ( inter.specularImage ) {
            RB_SubmittInteraction(&inter, DrawInteraction);
          }

          RB_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.specularImage,
                                inter.specularMatrix, inter.specularColor.ToFloatPtr());
          inter.specularColor[0] *= lightColor[0];
          inter.specularColor[1] *= lightColor[1];
          inter.specularColor[2] *= lightColor[2];
          inter.specularColor[3] *= lightColor[3];
          inter.vertexColor = surfaceStage->vertexColor;
          break;
        }
      }
    }

    // draw the final interaction
    RB_SubmittInteraction(&inter, DrawInteraction);
  }

  // unhack depth range if needed
  if ( surf->space->weaponDepthHack || surf->space->modelDepthHack != 0.0f ) {
    RB_GLSL_LeaveDepthHack(surf);
  }
}

/*
=============
RB_GLSL_CreateDrawInteractions

=============
*/
static void RB_GLSL_CreateDrawInteractions(const drawSurf_t* surf, const int depthFunc = GLS_DEPTHFUNC_EQUAL) {
  if ( !surf ) {
    return;
  }

  // perform setup here that will be constant for all interactions
  GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | depthFunc);

  // bind the vertex and fragment shader
  if ( r_usePhong.GetBool()) {
    GL_UseProgram(&interactionPhongShader);
  }
  else {
    GL_UseProgram(&interactionShader);
  }

  // Enable the Vertex attributes arrays
  // Vertex attrib is already enabled
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Tangent));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Bitangent));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));

  for ( ; surf; surf = surf->nextOnLight ) {
    // perform setup here that will not change over multiple interaction passes

    float mat[16];
    myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);

    // set the vertex pointers
    idDrawVert* ac = (idDrawVert*) vertexCache.Position(surf->geo->ambientCache);

    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Normal), 3, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->normal.ToFloatPtr());
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Bitangent), 3, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->tangents[1].ToFloatPtr());
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Tangent), 3, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->tangents[0].ToFloatPtr());
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 2, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->st.ToFloatPtr());
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 3, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->xyz.ToFloatPtr());
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Color), 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert),
                           (void*) &ac->color);

    // this may cause RB_GLSL_DrawInteraction to be exacuted multiple
    // times with different colors and images if the surface or light have multiple layers
    RB_GLSL_CreateSingleDrawInteractions(surf, RB_GLSL_DrawInteraction);
  }

  // Disable the Vertex attributes arrays
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Tangent));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Bitangent));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));

  // Cleanup the texture bindings
  GL_SelectTexture(4);
  globalImages->BindNull();

  GL_SelectTexture(3);
  globalImages->BindNull();

  GL_SelectTexture(2);
  globalImages->BindNull();

  GL_SelectTexture(1);
  globalImages->BindNull();

  GL_SelectTexture(0);
  globalImages->BindNull();

  GL_UseProgram(NULL);
}

/*
==================
RB_GLSL_DrawInteractions
==================
*/
void RB_GLSL_DrawInteractions(void) {

  ///////////////////////
  // For each light loop
  ///////////////////////

  viewLight_t* vLight;
  //
  // for each light, perform adding and shadowing
  //
  for ( vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next ) {

    //////////////
    // Skip Cases
    //////////////

    // do fogging later
    if ( vLight->lightShader->IsFogLight()) {
      continue;
    }

    if ( vLight->lightShader->IsBlendLight()) {
      continue;
    }

    if ( !vLight->localInteractions && !vLight->globalInteractions
         && !vLight->translucentInteractions ) {
      continue;
    }

    //////////////////
    // Setup GL state
    //////////////////

    // Current light
    backEnd.vLight = vLight;

    // clear the stencil buffer if needed
    if ( vLight->globalShadows || vLight->localShadows ) {

      if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals(vLight->scissorRect)) {
        backEnd.currentScissor = vLight->scissorRect;
        if (( backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1 ) < 0.0f ||
            ( backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 ) < 0.0f ) {
          backEnd.currentScissor = backEnd.viewDef->scissor;
        }
        qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                   backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                   backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                   backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
      }

      qglClear(GL_STENCIL_BUFFER_BIT);
    }
    else {
      // no shadows, so no need to read or write the stencil buffer
      // we might in theory want to use GL_ALWAYS instead of disabling
      // completely, to satisfy the invarience rules
      qglStencilFunc(GL_ALWAYS, 128, 255);
    }

    RB_GLSL_StencilShadowPass(vLight->globalShadows);
    RB_GLSL_CreateDrawInteractions(vLight->localInteractions);
    RB_GLSL_StencilShadowPass(vLight->localShadows);
    RB_GLSL_CreateDrawInteractions(vLight->globalInteractions);

    // translucent surfaces never get stencil shadowed
    if ( r_skipTranslucent.GetBool()) {
      continue;
    }

    qglStencilFunc(GL_ALWAYS, 128, 255);
    RB_GLSL_CreateDrawInteractions(vLight->translucentInteractions, GLS_DEPTHFUNC_LESS);
  }

  // disable stencil shadow test
  qglStencilFunc(GL_ALWAYS, 128, 255);
}

// NB: oh, a nice global variable. Argh....
static idPlane fogPlanes[4];

/*
=====================
RB_T_BasicFog
=====================
*/
static void RB_T_GLSL_BasicFog(const drawSurf_t* surf) {
  if ( backEnd.currentSpace != surf->space ) {
    idPlane local;

    //S
    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[0], local);
    local[3] += 0.5;
    GL_Uniform4fv(offsetof(shaderProgram_t, texGen0S), local.ToFloatPtr());

    //T
    local[0] = local[1] = local[2] = 0;
    local[3] = 0.5;
    GL_Uniform4fv(offsetof(shaderProgram_t, texGen0T), local.ToFloatPtr());

    //T
    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[2], local);
    local[3] += FOG_ENTER;
    GL_Uniform4fv(offsetof(shaderProgram_t, texGen1T), local.ToFloatPtr());

    //S
    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[3], local);
    GL_Uniform4fv(offsetof(shaderProgram_t, texGen1S), local.ToFloatPtr());
  }

  idDrawVert* ac = (idDrawVert*) vertexCache.Position(surf->geo->ambientCache);

  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 3, GL_FLOAT, false, sizeof(idDrawVert),
                         ac->xyz.ToFloatPtr());

  RB_DrawElementsWithCounters(surf->geo);
}

/*
======================
RB_RenderDrawSurfChainWithFunction
======================
*/
void RB_GLSL_RenderDrawSurfChainWithFunction(const drawSurf_t* drawSurfs,
                                             void (* triFunc_)(const drawSurf_t*)) {
  const drawSurf_t* drawSurf;

  backEnd.currentSpace = NULL;

  for ( drawSurf = drawSurfs; drawSurf; drawSurf = drawSurf->nextOnLight ) {

    // change the MVP matrix if needed
    if ( drawSurf->space != backEnd.currentSpace ) {
      float mat[16];
      myGlMultMatrix(drawSurf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
      GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
    }

    if ( drawSurf->space->weaponDepthHack ) {
      RB_GLSL_EnterWeaponDepthHack(drawSurf);
    }

    if ( drawSurf->space->modelDepthHack != 0.0f ) {
      RB_GLSL_EnterModelDepthHack(drawSurf);
    }

    // change the scissor if needed
    if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals(drawSurf->scissorRect)) {
      backEnd.currentScissor = drawSurf->scissorRect;
      if (( backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1 ) < 0.0f ||
          ( backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 ) < 0.0f ) {
        backEnd.currentScissor = backEnd.viewDef->scissor;
      }
      qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                 backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                 backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                 backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);

    }

    // render it
    triFunc_(drawSurf);

    if ( drawSurf->space->weaponDepthHack || drawSurf->space->modelDepthHack != 0.0f ) {
      RB_GLSL_LeaveDepthHack(drawSurf);
    }

    backEnd.currentSpace = drawSurf->space;
  }
}

/*
==================
RB_FogPass
==================
*/
void RB_GLSL_FogPass(const drawSurf_t* drawSurfs, const drawSurf_t* drawSurfs2) {

  drawSurf_t ds;
  const idMaterial* lightShader;
  const shaderStage_t* stage;
  const float* regs;

  // create a surface for the light frustom triangles, which are oriented drawn side out
  const srfTriangles_t* frustumTris = backEnd.vLight->frustumTris;

  // if we ran out of vertex cache memory, skip it
  if ( !frustumTris->ambientCache ) {
    return;
  }

  // Initial expected GL state:
  // Texture 0 is active, and bound to NULL
  // Vertex attribute array is enabled
  // All other attributes array are disabled
  // No shaders active

  GL_UseProgram(&fogShader);

  memset(&ds, 0, sizeof(ds));
  ds.space = &backEnd.viewDef->worldSpace;
  ds.geo = frustumTris;
  ds.scissorRect = backEnd.viewDef->scissor;

  // find the current color and density of the fog
  lightShader = backEnd.vLight->lightShader;
  regs = backEnd.vLight->shaderRegisters;
  // assume fog shaders have only a single stage
  stage = lightShader->GetStage(0);

  backEnd.lightColor[0] = regs[stage->color.registers[0]];
  backEnd.lightColor[1] = regs[stage->color.registers[1]];
  backEnd.lightColor[2] = regs[stage->color.registers[2]];
  backEnd.lightColor[3] = regs[stage->color.registers[3]];

  // FogColor
  GL_Uniform4fv(offsetof(shaderProgram_t, fogColor), backEnd.lightColor);

  // calculate the falloff planes
  const float a = ( backEnd.lightColor[3] <= 1.0 ) ? -0.5f / DEFAULT_FOG_DISTANCE : -0.5f / backEnd.lightColor[3];

  // texture 0 is the falloff image
  globalImages->fogImage->Bind();

  fogPlanes[0][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[2];
  fogPlanes[0][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[6];
  fogPlanes[0][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[10];
  fogPlanes[0][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[14];

  fogPlanes[1][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[0];
  fogPlanes[1][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[4];
  fogPlanes[1][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[8];
  fogPlanes[1][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[12];

  // texture 1 is the entering plane fade correction
  GL_SelectTexture(1);
  globalImages->fogEnterImage->Bind();
  GL_SelectTexture(0);

  // T will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
  fogPlanes[2][0] = 0.001f * backEnd.vLight->fogPlane[0];
  fogPlanes[2][1] = 0.001f * backEnd.vLight->fogPlane[1];
  fogPlanes[2][2] = 0.001f * backEnd.vLight->fogPlane[2];
  fogPlanes[2][3] = 0.001f * backEnd.vLight->fogPlane[3];

  // S is based on the view origin
  const float s = backEnd.viewDef->renderView.vieworg * fogPlanes[2].Normal() + fogPlanes[2][3];
  fogPlanes[3][0] = 0;
  fogPlanes[3][1] = 0;
  fogPlanes[3][2] = 0;
  fogPlanes[3][3] = FOG_ENTER + s;

  // draw it
  GL_State(GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL);
  RB_GLSL_RenderDrawSurfChainWithFunction(drawSurfs, RB_T_GLSL_BasicFog);
  RB_GLSL_RenderDrawSurfChainWithFunction(drawSurfs2, RB_T_GLSL_BasicFog);

  // the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
  // of depthfunc_equal
  GL_State(GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS);
  GL_Cull(CT_BACK_SIDED);
  RB_GLSL_RenderDrawSurfChainWithFunction(&ds, RB_T_GLSL_BasicFog);
  GL_Cull(CT_FRONT_SIDED);
  GL_State(GLS_DEPTHMASK | GLS_DEPTHFUNC_EQUAL); // Restore DepthFunc

  // Restore GL State that might have been changed locally:
  // Tex1 image set to NULL
  // Shader set to NULL
  //
  // Restore GL State that have been changed in submethods:
  // Tex0 image set to NULL
  //
  // We don't care about uniform states, and vertex attributes pointers

  GL_SelectTexture(1);
  globalImages->BindNull();
  GL_SelectTexture(0);

  GL_UseProgram(NULL);

  globalImages->BindNull();
}

/*
======================
RB_GLSL_LoadShaderTextureMatrix
======================
*/
void RB_GLSL_LoadShaderTextureMatrix(const float* shaderRegisters, const textureStage_t* texture) {
  float matrix[16];
  RB_GetShaderTextureMatrix(shaderRegisters, texture, matrix);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), matrix);
}

/*
==================
RB_T_FillDepthBuffer
==================
*/
void RB_T_GLSL_FillDepthBuffer(const drawSurf_t* surf) {

  const idMaterial* const shader = surf->material;

  //////////////
  // Skip cases
  //////////////

  if ( !shader->IsDrawn()) {
    return;
  }

  const srfTriangles_t* const tri = surf->geo;

  // some deforms may disable themselves by setting numIndexes = 0
  if ( !tri->numIndexes ) {
    return;
  }

  // translucent surfaces don't put anything in the depth buffer and don't
  // test against it, which makes them fail the mirror clip plane operation
  if ( shader->Coverage() == MC_TRANSLUCENT ) {
    return;
  }

  if ( !tri->ambientCache ) {
    return;
  }

  // get the expressions for conditionals / color / texcoords
  const float* const regs = surf->shaderRegisters;

  // if all stages of a material have been conditioned off, don't do anything
  int stage;
  for ( stage = 0; stage < shader->GetNumStages(); stage++ ) {
    const shaderStage_t* pStage = shader->GetStage(stage);

    // check the stage enable condition
    if ( regs[pStage->conditionRegister] != 0 ) {
      break;
    }
  }

  if ( stage == shader->GetNumStages()) {
    return;
  }

  ///////////////////////////////////////////
  // GL Shader setup for the current surface
  ///////////////////////////////////////////

  // Invariants (each time we are here, these should be satisfied):
  // Active shader: zfill or zfillClip
  // Tex0: active, and bound to whiteImage
  // Tex1: if zfillClip shader, bound to alphaNotchImage
  // Texture matrix: identity
  // Alpha test: always pass
  // DepthFunc: LESS
  // PolygonOffset: disabled
  // StencilTest: enabled
  // MVP: properly set
  // Blend mode

  // Varying (they will change, and we don't care):
  // Clip plane
  // VertexAttribPointer
  // TexCoordAttribPointer

  // update the clip plane if needed
  if ( backEnd.viewDef->numClipPlanes && surf->space != backEnd.currentSpace ) {
    idPlane plane;

    R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.viewDef->clipPlanes[0], plane);
    plane[3] += 0.5;  // the notch is in the middle

    GL_Uniform4fv(offsetof(shaderProgram_t, texGen0S), plane.ToFloatPtr());
  }

  // set polygon offset if necessary
  // NB: will be restored at the end of the process
  if ( shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglEnable(GL_POLYGON_OFFSET_FILL);
    qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
  }

  // Color
  // black by default
  float color[4] = { 0, 0, 0, 1 };
  // subviews will just down-modulate the color buffer by overbright
  // NB: will be restored at end of the process
  if ( shader->GetSort() == SS_SUBVIEW ) {
    GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS);
    color[0] = color[1] = color[2] = 1.0f; // NB: was 1.0 / backEnd.overBright );
  }
  GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);

  // Get vertex data
  idDrawVert* ac = (idDrawVert*) vertexCache.Position(tri->ambientCache);

  // Setup attribute pointers
  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 3, GL_FLOAT, false, sizeof(idDrawVert),
                         ac->xyz.ToFloatPtr());
  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 2, GL_FLOAT, false, sizeof(idDrawVert),
                         ac->st.ToFloatPtr());

  bool drawSolid = false;

  if ( shader->Coverage() == MC_OPAQUE ) {
    drawSolid = true;
  }

  ////////////////////////////////
  // Perforated surfaces handling
  ////////////////////////////////

  // we may have multiple alpha tested stages
  if ( shader->Coverage() == MC_PERFORATED ) {
    // if the only alpha tested stages are condition register omitted,
    // draw a normal opaque surface
    bool didDraw = false;

    ///////////////////////
    // For each stage loop
    ///////////////////////

    // perforated surfaces may have multiple alpha tested stages
    for ( stage = 0; stage < shader->GetNumStages(); stage++ ) {
      const shaderStage_t* pStage = shader->GetStage(stage);

      //////////////
      // Skip cases
      //////////////

      if ( !pStage->hasAlphaTest ) {
        continue;
      }

      // check the stage enable condition
      if ( regs[pStage->conditionRegister] == 0 ) {
        continue;
      }

      // if we at least tried to draw an alpha tested stage,
      // we won't draw the opaque surface
      didDraw = true;

      // set the alpha modulate
      color[3] = regs[pStage->color.registers[3]];

      // skip the entire stage if alpha would be black
      if ( color[3] <= 0 ) {
        continue;
      }

      //////////////////////////
      // GL Setup for the stage
      //////////////////////////

      // Invariants (each time we are here, these should be satisfied):
      // Active shader: zfill or zfillClip
      // Tex0: active
      // Tex1: if zfillClip shader, bound to alphaNotchImage
      // Texture matrix: identity
      // DepthFunc: LESS
      // PolygonOffset: disabled
      // StencilTest: enabled
      // MVP: properly set
      // Clip plane
      // VertexAttribPointer
      // TexCoordAttribPointer
      // Blend mode

      // Varying (they will change, and we don't care):
      // Tex0 binding, color, alphatest

      // Color
      // alpha testing
      GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);
      GL_Uniform1fv(offsetof(shaderProgram_t, alphaTest), &regs[pStage->alphaTestRegister]);

      // bind the texture
      pStage->texture.image->Bind();

      // Setup the texture matrix if needed
      // NB: will be restored to identity
      if ( pStage->texture.hasMatrix ) {
        RB_GLSL_LoadShaderTextureMatrix(surf->shaderRegisters, &pStage->texture);
      }

      ///////////
      // Draw it
      ///////////
      RB_DrawElementsWithCounters(tri);

      ////////////////////////////////////////////////////////////
      // Restore everything to an acceptable state for next stage
      ////////////////////////////////////////////////////////////

      // Invariants to mach that may have changed:
      // Texture Matrix

      // Restore identity matrix
      if ( pStage->texture.hasMatrix ) {
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
      }
    }

    if ( !didDraw ) {
      drawSolid = true;
    }

    ///////////////////////////////////////////////////////////
    // Restore everything to an acceptable state for next step
    ///////////////////////////////////////////////////////////

    // Invariants to match that may have changed:
    // Color
    // Alpha esting
    // Tex0 binding

    // Restore color alpha
    color[3] = 1;
    GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);

    // Restore alphatest always passing
    static const float one[4] = { 1, 1, 1, 1 };
    GL_Uniform1fv(offsetof(shaderProgram_t, alphaTest), one);

    // Restore white image binding to Tex0
    globalImages->whiteImage->Bind();
  }

  ////////////////////////////////////////
  // Normal surfaces case (non perforated)
  ////////////////////////////////////////

  // Invariants (each time we are here, these should be satisfied):
  // Active shader: zfill or zfillClip
  // Tex0: active and bound to white image
  // Tex1: if zfillClip shader, bound to alphaNotchImage
  // Texture matrix: identity
  // DepthFunc: LESS
  // PolygonOffset: disabled
  // StencilTest: enabled
  // MVP: properly set
  // Clip plane
  // VertexAttribPointer
  // TexCoordAttribPointer
  // Color

  // draw the entire surface solid
  if ( drawSolid ) {
    ///////////
    // Draw it
    ///////////
    RB_DrawElementsWithCounters(tri);
  }

  /////////////////////////////////////////////
  // Restore everything to an acceptable state
  /////////////////////////////////////////////

  // Invariants to mach that may have changed
  // Polygon offset
  // Blending mode

  // reset polygon offset
  if ( shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglDisable(GL_POLYGON_OFFSET_FILL);
  }

  // Restore blending
  if ( shader->GetSort() == SS_SUBVIEW ) {
    GL_State(GLS_DEPTHFUNC_LESS);
  }
}

/*
=====================
RB_GLSL_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_GLSL_FillDepthBuffer(drawSurf_t** drawSurfs, int numDrawSurfs) {

  //////////////
  // Skip cases
  //////////////

  // if we are just doing 2D rendering, no need to fill the depth buffer
  if ( !backEnd.viewDef->viewEntitys ) {
    return;
  }

  ////////////////////////////////////////
  // GL Shader setup for the current pass
  // (ie. common to each surface)
  ////////////////////////////////////////

  // Invariants (each time we are here, these should be satisfied):
  // No shaders active
  // Tex0 active, and bound to NULL
  // Tex1 bound to NULL

  // If clip planes are enabled in the view, use he "Clip" version of zfill shader
  // and enable the second texture for mirror plane clipping if needed
  if ( backEnd.viewDef->numClipPlanes ) {
    // Use he zfillClip shader
    GL_UseProgram(&zfillClipShader);

    // Bind the Texture 1 to alphaNotchImage
    GL_SelectTexture(1);
    globalImages->alphaNotchImage->Bind();

    // Be sure to reactivate Texture 0, as it will be bound right after
    GL_SelectTexture(0);
  }
    // If no clip planes, just use the regular zfill shader
  else {
    GL_UseProgram(&zfillShader);
  }

  // Enable the Vertex attributes arrays
  // Vertex attribute is always enabled
  // TexCoords
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));

  // Texture 0 will be used for alpha tested surfaces. It should be already active.
  // Bind it to white image by default
  globalImages->whiteImage->Bind();

  // Load identity matrix for Texture marix
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());

  // Alpha test always pass by default
  static const GLfloat one[1] = { 1 };
  GL_Uniform1fv(offsetof(shaderProgram_t, alphaTest), one);

  // Decal surfaces may enable polygon offset
  qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat());

  // Depth func to LESS
  GL_State(GLS_DEPTHFUNC_LESS);

  // Enable stencil test if we are going to be using it for shadows.
  // If we didn't do this, it would be legal behavior to get z fighting
  // from the ambient pass and the light passes.
  qglEnable(GL_STENCIL_TEST);
  qglStencilFunc(GL_ALWAYS, 1, 255);

  //////////////////////////
  // For each surfaces loop
  //////////////////////////

  // Optimization to only change MVP matrix when needed
  backEnd.currentSpace = NULL;

  for ( int i = 0; i < numDrawSurfs; i++ ) {

    const drawSurf_t* const drawSurf = drawSurfs[i];

    ///////////////////////////////////////////
    // GL shader setup for the current surface
    ///////////////////////////////////////////

    // Invariants (each time we are here, these should be satisfied):
    // Active shader: zfill or zfillClip
    // Tex0: active, and bound to whiteImage
    // Tex1: if zfillClip shader, bound to alphaNotchImage
    // Attributes arrays enabled: Vertex, TexCoord
    // Texture matrix: identity
    // Alpha test: always pass
    // DepthFunc: LESS
    // PolygonOffset: disabled
    // StencilTest: enabled

    // change the MVP matrix if needed
    if ( drawSurf->space != backEnd.currentSpace ) {
      float mat[16];
      myGlMultMatrix(drawSurf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
      GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
    }

    // Hack the MVP matrix if needed
    if ( drawSurf->space->weaponDepthHack ) {
      RB_GLSL_EnterWeaponDepthHack(drawSurf);
    }

    if ( drawSurf->space->modelDepthHack != 0.0f ) {
      RB_GLSL_EnterModelDepthHack(drawSurf);
    }

    // change the scissor if needed
    if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals(drawSurf->scissorRect)) {
      backEnd.currentScissor = drawSurf->scissorRect;
      if (( backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1 ) < 0.0f ||
          ( backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 ) < 0.0f ) {
        backEnd.currentScissor = backEnd.viewDef->scissor;
      }
      qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                 backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                 backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                 backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
    }

    ////////////////////
    // Do the real work
    ////////////////////
    RB_T_GLSL_FillDepthBuffer(drawSurf);

    /////////////////////////////////////////////
    // Restore everything to an acceptable state
    /////////////////////////////////////////////

    // Invariants to match that may have changed:
    // none

    // Well... restore the MVP matrix from its hacked version if needed
    // This is in case we don't have changed space at next iteration, but we are no more in depth hack mode
    if ( drawSurf->space->weaponDepthHack || drawSurf->space->modelDepthHack != 0.0f ) {
      RB_GLSL_LeaveDepthHack(drawSurf);
    }

    // Let's change space for next iteration
    backEnd.currentSpace = drawSurf->space;
  }

  /////////////////////////////////////////////
  // Restore everything to an acceptable state
  /////////////////////////////////////////////
  // Restore current space to NULL
  backEnd.currentSpace = NULL;

  // Disable TexCoord array
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));

  // Invariants to match that may have changed:
  // Tex1 bound to NULL
  // Tex0 active, and bound to NULL
  // No shaders active

  // Bind Tex1 to NULL
  if ( backEnd.viewDef->numClipPlanes ) {
    GL_SelectTexture(1);
    globalImages->BindNull();

    GL_SelectTexture(0);
  }

  // Bind Tex0 to NULL
  globalImages->BindNull();

  // Reset the shader
  GL_UseProgram(NULL);
}

/*
==================
RB_GLSL_T_RenderShaderPasses

This is also called for the generated 2D rendering
==================
*/
void RB_GLSL_T_RenderShaderPasses(const drawSurf_t* surf) {

  // global constants
  static const float zero[4] = { 0, 0, 0, 0 };
  static const float one[4] = { 1, 1, 1, 1 };
  static const float negOne[4] = { -1, -1, -1, -1 };

  // usefull pointers
  const idMaterial* const shader = surf->material;
  const srfTriangles_t* const tri = surf->geo;

  //////////////
  // Skip cases
  //////////////

  if ( !shader->HasAmbient()) {
    return;
  }

  if ( shader->IsPortalSky()) {
    return;
  }

  // some deforms may disable themselves by setting numIndexes = 0
  if ( !tri->numIndexes ) {
    return;
  }

  if ( !tri->ambientCache ) {
    return;
  }

  ///////////////////////////////////
  // GL shader setup for the surface
  // (ie. common to each Stage)
  ///////////////////////////////////

  // change the scissor if needed
  if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals(surf->scissorRect)) {
    backEnd.currentScissor = surf->scissorRect;
    if (( backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1 ) < 0.0f ||
        ( backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 ) < 0.0f ) {
      backEnd.currentScissor = backEnd.viewDef->scissor;
    }
    qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
               backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
               backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
               backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);

  }

  // set polygon offset if necessary
  // NB: must be restored at end of process
  if ( shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglEnable(GL_POLYGON_OFFSET_FILL);
    qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
  }

  // set face culling appropriately
  GL_Cull(shader->GetCullType());

  // Quick and dirty hacks on the depth range
  // NB: must be restored at end of process
  if ( surf->space->weaponDepthHack && surf->space->modelDepthHack == 0.0f ) {
    qglDepthRangef(0, 0.5);
  }

  // Location of vertex attributes data
  const idDrawVert* const ac = (const idDrawVert* const) vertexCache.Position(tri->ambientCache);

  // get the expressions for conditionals / color / texcoords
  const float* const regs = surf->shaderRegisters;

  // Additional precomputations that will be reused in the shader stages

  // precompute the projection matrix
  float localProjectionMatrix[16];
  memcpy(localProjectionMatrix, backEnd.viewDef->projectionMatrix, sizeof(localProjectionMatrix));
  const float v = localProjectionMatrix[14];

  // Quick and dirty hacks on the projection marix
  if ( surf->space->weaponDepthHack ) {
    localProjectionMatrix[14] = v * 0.25;
  }
  if ( surf->space->modelDepthHack != 0.0 ) {
    localProjectionMatrix[14] = v - surf->space->modelDepthHack;
  }

  // precompute the MVP
  float localMVP[16];
  myGlMultMatrix(surf->space->modelViewMatrix, localProjectionMatrix, localMVP);

  // precompute the local view origin (might be needed for some texgens)
  idVec4 localViewOrigin;
  R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3());
  localViewOrigin.w = 1.0f;

  ///////////////////////
  // For each stage loop
  ///////////////////////
  for ( int stage = 0; stage < shader->GetNumStages(); stage++ ) {

    const shaderStage_t* const pStage = shader->GetStage(stage);

    ///////////////
    // Skip cases
    ///////////////

    // check the enable condition
    if ( regs[pStage->conditionRegister] == 0 ) {
      continue;
    }

    // skip the stages involved in lighting
    if ( pStage->lighting != SL_AMBIENT ) {
      continue;
    }

    // skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
    if (( pStage->drawStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS )) ==
        ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE )) {
      continue;
    }

    // see if we are a new-style stage
    const newShaderStage_t* const newStage = pStage->newStage;

    if ( newStage ) {
      // new style stages: Not implemented in GLSL yet!
      continue;
    }
    else {

      // old style stages

      /////////////////////////
      // Additional skip cases
      /////////////////////////

      // precompute the color
      const float color[4] = {
        regs[pStage->color.registers[0]],
        regs[pStage->color.registers[1]],
        regs[pStage->color.registers[2]],
        regs[pStage->color.registers[3]]
      };

      // skip the entire stage if an add would be black
      if (
        ( pStage->drawStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS )) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE )
        && color[0] <= 0 && color[1] <= 0 && color[2] <= 0 ) {
        continue;
      }

      // skip the entire stage if a blend would be completely transparent
      if (( pStage->drawStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS )) ==
          ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
          && color[3] <= 0 ) {
        continue;
      }

      /////////////////////////////////
      // GL shader setup for the stage
      /////////////////////////////////
      // The very first thing we need to do before going down into GL is to choose he correct GLSL shader depending on
      // the associated TexGen. Then, setup its specific uniforms/attribs, and then only we can setup the common uniforms/attribs

      if ( pStage->texture.texgen == TG_DIFFUSE_CUBE ) {
        // Not used in game, but implemented because trivial

        // This is diffuse cube mapping
        GL_UseProgram(&diffuseCubeShader);

        // Possible that normals should be transformed by a normal matrix in the shader ? I am not sure...

        // Setup normal array
        GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
        GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Normal), 3, GL_FLOAT, false, sizeof(idDrawVert),
                               ac->normal.ToFloatPtr());

        // Setup the texture matrix to identity
        // Note: not sure, in original D3 it looks like having diffuse cube with texture matrix other than identity is a possible case.
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
      }
      else if ( pStage->texture.texgen == TG_SKYBOX_CUBE ) {
        // This is skybox cube mapping
        GL_UseProgram(&skyboxCubeShader);

        // Setup the local view origin uniform
        GL_Uniform4fv(offsetof(shaderProgram_t, localViewOrigin), localViewOrigin.ToFloatPtr());

        // Setup the texture matrix to identity
        // Note: not sure, in original D3 it looks like having skyboxes with texture matrix other than identity is a possible case.
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
      }
      else if ( pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {
        // This is skybox cube mapping, with special texture matrix
        GL_UseProgram(&skyboxCubeShader);

        // Setup the local view origin uniform
        GL_Uniform4fv(offsetof(shaderProgram_t, localViewOrigin), localViewOrigin.ToFloatPtr());

        // Setup the specific texture matrix for the wobblesky
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), surf->wobbleTransform);
      }
      else if ( pStage->texture.texgen == TG_SCREEN ) {
        // Not used in game, so not implemented
        continue;
      }
      else if ( pStage->texture.texgen == TG_SCREEN2 ) {
        // Not used in game, so not implemented
        continue;
      }
      else if ( pStage->texture.texgen == TG_GLASSWARP ) {
        // Not used in game, so not implemented. The shader code is even not present in original D3 data
        continue;
      }
      else if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
        // This is reflection cubemapping
        GL_UseProgram(&reflectionCubeShader);

        // NB: in original D3, if the surface had a bump map it would lead to the "Bumpy reflection cubemaping" shader being used.
        // This is not implemented for now, we only do standard reflection cubemaping. Visual difference is really minor.

        GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));

        // Setup the normals
        GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Normal), 3, GL_FLOAT, false, sizeof(idDrawVert),
                               ac->normal.ToFloatPtr());

        // Setup the modelViewMatrix, we will need it to compute the reflection
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), surf->space->modelViewMatrix);

        // Setup the texture matrix like original D3 code does: using the transpose modelViewMatrix
        // NB: this is curious, not sure why this is done like this....
        float mat[16];
        R_TransposeGLMatrix(backEnd.viewDef->worldSpace.modelViewMatrix, mat);
        GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat);
      }
      else {  // TG_EXPLICIT
        // Otherwise, this is just regular surface shader with explicit texcoords
        GL_UseProgram(&defaultSurfaceShader);

        GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));

        // Setup the TexCoord pointer
        GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 2, GL_FLOAT, false, sizeof(idDrawVert),
                               ac->st.ToFloatPtr());

        // Setup the texture matrix
        if ( pStage->texture.hasMatrix ) {
          RB_GLSL_LoadShaderTextureMatrix(surf->shaderRegisters, &pStage->texture);
        }
        else {
          // Use identity
          GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
        }
      }

      // Now we have a shader, we can setup the uniforms and attribute pointers common to all kind of shaders
      // The specifics have already been done in the shader selection code (see above)

      // Setup the Vertex Attrib pointer
      GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 3, GL_FLOAT, false, sizeof(idDrawVert),
                             ac->xyz.ToFloatPtr());

      // Setup the MVP uniform
      GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), localMVP);

      // Setup the Color uniform
      GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);

      GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));

      // Setup the Color pointer
      GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Color), 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert),
                             (void*) &ac->color);

      // Setup the Color modulation
      switch ( pStage->vertexColor ) {
        case SVC_MODULATE:
          GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), one);
          GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), zero);
          break;
        case SVC_INVERSE_MODULATE:
          GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), negOne);
          GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
          break;
        case SVC_IGNORE:
        default:
          GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), zero);
          GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
          break;
      }

      // bind the texture (this will be either a dynamic texture, or a static one)
      RB_BindVariableStageImage(&pStage->texture, regs);

      // set the state
      GL_State(pStage->drawStateBits);

      // set privatePolygonOffset if necessary
      if ( pStage->privatePolygonOffset ) {
        qglEnable(GL_POLYGON_OFFSET_FILL);
        qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset);
      }

      /////////////////////
      // Draw the surface!
      /////////////////////
      RB_DrawElementsWithCounters(tri);

      /////////////////////////////////////////////
      // Restore everything to an acceptable state
      /////////////////////////////////////////////

      // Disable color attributes array
      GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));

      // Disable the other attributes array
      if ( pStage->texture.texgen == TG_DIFFUSE_CUBE ) {
        GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
      }
      else if ( pStage->texture.texgen == TG_SKYBOX_CUBE ) {
      }
      else if ( pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {
      }
      else if ( pStage->texture.texgen == TG_SCREEN ) {
      }
      else if ( pStage->texture.texgen == TG_SCREEN2 ) {
      }
      else if ( pStage->texture.texgen == TG_GLASSWARP ) {
      }
      else if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
        GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
      }
      else {
        GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));
      }

      // unset privatePolygonOffset if necessary
      if ( pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
        qglDisable(GL_POLYGON_OFFSET_FILL);
      }
      else if ( pStage->privatePolygonOffset && surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
        qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
      }

      // Don't touch the rest, as this will either reset by the next stage, or handled by end of this method
    }
  }

  /////////////////////////////////////////////
  // Restore everything to an acceptable state
  /////////////////////////////////////////////

  // reset polygon offset
  if ( shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglDisable(GL_POLYGON_OFFSET_FILL);
  }

  // Restore the depth range if it was hacked somehow
  if ( surf->space->weaponDepthHack ) {
    qglDepthRangef(0.0f, 1.0f);
  }

  // Don't touch the rest, as this will be reset by caller method

  // CAUTION: potentially trashed GL state with the call to GL_State(pStage->drawStateBits) in the stages
  // loop. But seems ok actually

  return; // pheww....
}

/*
=====================
RB_GLSL_DrawShaderPasses

Draw non-light dependent passes
=====================
*/
int RB_GLSL_DrawShaderPasses(drawSurf_t** drawSurfs, int numDrawSurfs) {

  //////////////
  // Skip cases
  //////////////

  // only obey skipAmbient if we are rendering a view
  if ( backEnd.viewDef->viewEntitys && r_skipAmbient.GetBool()) {
    return numDrawSurfs;
  }

  // if we are about to draw the first surface that needs
  // the rendering in a texture, copy it over
  if ( drawSurfs[0]->material->GetSort() >= SS_POST_PROCESS ) {
    if ( r_skipPostProcess.GetBool()) {
      return 0;
    }

    // only dump if in a 3d view
    if ( backEnd.viewDef->viewEntitys ) {
      globalImages->currentRenderImage->CopyFramebuffer(backEnd.viewDef->viewport.x1,
                                                        backEnd.viewDef->viewport.y1,
                                                        backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1,
                                                        backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1,
                                                        true);
    }

    backEnd.currentRenderCopied = true;
  }

  ////////////////////////////////////////
  // GL shader setup for the current pass
  // (ie. common to each surface)
  ////////////////////////////////////////

  // Invariants (each time we are here, these should be satisfied):
  // No shaders active
  // Tex0 active, and bound to NULL

  // Actually, no GL shader setup can be done there, as the shader to use is dependent on the surface shader stages

  /////////////////////////
  // For each surface loop
  /////////////////////////

  int i;
  for ( i = 0; i < numDrawSurfs; i++ ) {

    //////////////
    // Skip cases
    //////////////

    if ( drawSurfs[i]->material->SuppressInSubview()) {
      continue;
    }

    if ( backEnd.viewDef->isXraySubview && drawSurfs[i]->space->entityDef ) {
      if ( drawSurfs[i]->space->entityDef->parms.xrayIndex != 2 ) {
        continue;
      }
    }

    // we need to draw the post process shaders after we have drawn the fog lights
    if ( drawSurfs[i]->material->GetSort() >= SS_POST_PROCESS
         && !backEnd.currentRenderCopied ) {
      break;
    }

    ////////////////////
    // Do the real work
    ////////////////////
    RB_GLSL_T_RenderShaderPasses(drawSurfs[i]);
  }

  /////////////////////////////////////////////
  // Restore everything to an acceptable state
  /////////////////////////////////////////////

  // Invariants to match that may have changed:
  // Tex0 active, and bound to NULL
  // No shaders active
  // Culling

  // Restore culling
  GL_Cull(CT_FRONT_SIDED);

  // Bind Tex0 to NULL
  globalImages->BindNull();

  // Reset the shader
  GL_UseProgram(NULL);

  // Return the counter of drawn surfaces
  return i;
}

/*
=====================
RB_T_GLSL_Shadow

the shadow volumes face INSIDE
=====================
*/
static void RB_T_GLSL_Shadow(const drawSurf_t* surf) {
  const srfTriangles_t* tri = surf->geo;

  if ( !tri->shadowCache ) {
    return;
  }

  // set the light position for the vertex program to project the rear surfaces
  if ( surf->space != backEnd.currentSpace ) {
    idVec4 localLight;

    R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3());
    localLight.w = 0.0f;
    GL_Uniform4fv(offsetof(shaderProgram_t, localLightOrigin), localLight.ToFloatPtr());
  }

  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 4, GL_FLOAT, false, sizeof(shadowCache_t),
                         vertexCache.Position(tri->shadowCache));

  // we always draw the sil planes, but we may not need to draw the front or rear caps
  int numIndexes;
  bool external = false;

  if ( !r_useExternalShadows.GetInteger()) {
    numIndexes = tri->numIndexes;
  }
  else if ( r_useExternalShadows.GetInteger() == 2 ) {   // force to no caps for testing
    numIndexes = tri->numShadowIndexesNoCaps;
  }
  else if ( !( surf->dsFlags & DSF_VIEW_INSIDE_SHADOW )) {
    // if we aren't inside the shadow projection, no caps are ever needed needed
    numIndexes = tri->numShadowIndexesNoCaps;
    external = true;
  }
  else if ( !backEnd.vLight->viewInsideLight && !( surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE )) {
    // if we are inside the shadow projection, but outside the light, and drawing
    // a non-infinite shadow, we can skip some caps
    if ( backEnd.vLight->viewSeesShadowPlaneBits & surf->geo->shadowCapPlaneBits ) {
      // we can see through a rear cap, so we need to draw it, but we can skip the
      // caps on the actual surface
      numIndexes = tri->numShadowIndexesNoFrontCaps;
    }
    else {
      // we don't need to draw any caps
      numIndexes = tri->numShadowIndexesNoCaps;
    }

    external = true;
  }
  else {
    // must draw everything
    numIndexes = tri->numIndexes;
  }

  // depth-fail stencil shadows
  if ( !external ) {
    qglStencilOpSeparate(backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, GL_DECR, GL_KEEP);
    qglStencilOpSeparate(backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, GL_INCR, GL_KEEP);
  }
  else {
    // traditional depth-pass stencil shadows
    qglStencilOpSeparate(backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, GL_KEEP, GL_INCR);
    qglStencilOpSeparate(backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR);
  }
  GL_Cull(CT_TWO_SIDED);
  RB_DrawShadowElementsWithCounters(tri, numIndexes);

  // patent-free work around
  /*if (!external) {
    // "preload" the stencil buffer with the number of volumes
    // that get clipped by the near or far clip plane
    qglStencilOp(GL_KEEP, GL_DECR, GL_DECR);
    GL_Cull(CT_FRONT_SIDED);
    RB_DrawShadowElementsWithCounters(tri, numIndexes);
    qglStencilOp(GL_KEEP, GL_INCR, GL_INCR);
    GL_Cull(CT_BACK_SIDED);
    RB_DrawShadowElementsWithCounters(tri, numIndexes);
  }

  // traditional depth-pass stencil shadows
  qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
  GL_Cull(CT_FRONT_SIDED);
  RB_DrawShadowElementsWithCounters(tri, numIndexes);

  qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
  GL_Cull(CT_BACK_SIDED);
  RB_DrawShadowElementsWithCounters(tri, numIndexes);*/
}


/*
=====================
RB_GLSL_StencilShadowPass

Stencil test should already be enabled, and the stencil buffer should have
been set to 128 on any surfaces that might receive shadows
=====================
*/
void RB_GLSL_StencilShadowPass(const drawSurf_t* drawSurfs) {

  //////////////
  // Skip cases
  //////////////

  if ( !r_shadows.GetBool()) {
    return;
  }

  if ( !drawSurfs ) {
    return;
  }

  //////////////////
  // Setup GL state
  //////////////////

  // Initial expected GL state:
  // Texture 0 is active, and bound to NULL
  // No shaders active

  // Use the stencil shadow shader
  GL_UseProgram(&stencilShadowShader);

  // Enable the Vertex attributes arrays
  // NB: Vertex attrib is already enabled

  // don't write to the color buffer, just the stencil buffer
  GL_State(GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS);

  if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat()) {
    qglPolygonOffset(r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat());
    qglEnable(GL_POLYGON_OFFSET_FILL);
  }

  qglStencilFunc(GL_ALWAYS, 1, 255);

  RB_GLSL_RenderDrawSurfChainWithFunction(drawSurfs, RB_T_GLSL_Shadow);

  GL_Cull(CT_FRONT_SIDED);

  if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat()) {
    qglDisable(GL_POLYGON_OFFSET_FILL);
  }

  qglStencilFunc(GL_GEQUAL, 128, 255);
  qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // Restore GL State that might have been changed locally:
  // Shader set to NULL
  //
  // This pass does not use any vertex attribute (other than vertexes obviously), and no textures
  //
  // We don't care about uniform states

  // Desactivate the shader
  GL_UseProgram(NULL);
}
