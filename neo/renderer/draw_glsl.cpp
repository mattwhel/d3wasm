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

static const char* const interactionShaderVP =
    "#version 100\n"
    "\n"
    "precision highp float;\n"
    "\n"
    "/*\n"
    " * Pixel values between vertices are interpolated by Gouraud shading by default,\n"
    " * rather than the more computationally-expensive Phong shading.\n"
    " */\n"
    "//#define BLINN_PHONG\n"
    "\n"
    "varying vec2 var_TexDiffuse;\n"
    "varying vec2 var_TexNormal;\n"
    "varying vec2 var_TexSpecular;\n"
    "varying vec4 var_TexLight;\n"
    "varying mediump vec4 var_Color;\n"
    "varying vec3 var_L;\n"
    "varying vec3 var_V;\n"
    "#if defined(BLINN_PHONG)\n"
    "varying vec3 var_H;\n"
    "#endif\n"
    "\n"
    "attribute vec4 attr_TexCoord;\n"
    "attribute vec3 attr_Tangent;\n"
    "attribute vec3 attr_Bitangent;\n"
    "attribute vec3 attr_Normal;\n"
    "attribute vec4 attr_Vertex;\n"
    "attribute mediump vec4 attr_Color;\n"
    "\n"
    "uniform vec4 u_lightProjectionS;\n"
    "uniform vec4 u_lightProjectionT;\n"
    "uniform vec4 u_lightFalloff;\n"
    "uniform vec4 u_lightProjectionQ;\n"
    "uniform mediump vec4 u_colorModulate;\n"
    "uniform mediump vec4 u_colorAdd;\n"
    "uniform mediump vec4 u_glColor;\n"
    "\n"
    "uniform vec4 u_lightOrigin;\n"
    "uniform vec4 u_viewOrigin;\n"
    "\n"
    "uniform vec4 u_bumpMatrixS;\n"
    "uniform vec4 u_bumpMatrixT;\n"
    "uniform vec4 u_diffuseMatrixS;\n"
    "uniform vec4 u_diffuseMatrixT;\n"
    "uniform vec4 u_specularMatrixS;\n"
    "uniform vec4 u_specularMatrixT;\n"
    "\n"
    #ifdef USEREGAL
    "uniform mat4 u_modelViewMatrix;\n"
    "uniform mat4 u_projectionMatrix;\n"
    #else
    "uniform mat4 u_modelViewProjectionMatrix;\n"
    #endif
    "\n"
    "void main(void)\n"
    "{\n"
    "\tmat3 M = mat3(attr_Tangent, attr_Bitangent, attr_Normal);\n"
    "\n"
    "\tvar_TexNormal.x = dot(u_bumpMatrixS, attr_TexCoord);\n"
    "\tvar_TexNormal.y = dot(u_bumpMatrixT, attr_TexCoord);\n"
    "\n"
    "\tvar_TexDiffuse.x = dot(u_diffuseMatrixS, attr_TexCoord);\n"
    "\tvar_TexDiffuse.y = dot(u_diffuseMatrixT, attr_TexCoord);\n"
    "\n"
    "\tvar_TexSpecular.x = dot(u_specularMatrixS, attr_TexCoord);\n"
    "\tvar_TexSpecular.y = dot(u_specularMatrixT, attr_TexCoord);\n"
    "\n"
    "\tvar_TexLight.x = dot(u_lightProjectionS, attr_Vertex);\n"
    "\tvar_TexLight.y = dot(u_lightProjectionT, attr_Vertex);\n"
    "\tvar_TexLight.z = dot(u_lightFalloff, attr_Vertex);\n"
    "\tvar_TexLight.w = dot(u_lightProjectionQ, attr_Vertex);\n"
    "\n"
    "\tvec3 L = u_lightOrigin.xyz - attr_Vertex.xyz;\n"
    "\tvec3 V = u_viewOrigin.xyz - attr_Vertex.xyz;\n"
    "#if defined(BLINN_PHONG)\n"
    "\tvec3 H = normalize(L) + normalize(V);\n"
    "#endif\n"
    "\n"
    "\tvar_L = L * M;\n"
    "\tvar_V = V * M;\n"
    "#if defined(BLINN_PHONG)\n"
    "\tvar_H = H * M;\n"
    "#endif\n"
    "\n"
    "\tvar_Color = (attr_Color / 255.0) * u_colorModulate + u_colorAdd;\n"
    "\n"
    #ifdef USEREGAL
    "\tgl_Position = (u_projectionMatrix * u_modelViewMatrix ) * attr_Vertex;\n"
    #else
    "\tgl_Position = u_modelViewProjectionMatrix * attr_Vertex;\n"
    #endif
    "}\n";

static const char* const interactionShaderFP =
    "#version 100\n"
    "\n"
    "precision highp float;\n"
    "\n"
    "/*\n"
    " * Pixel values between vertices are interpolated by Gouraud shading by default,\n"
    " * rather than the more computationally-expensive Phong shading.\n"
    " */\n"
    "//#define BLINN_PHONG\n"
    "\n"
    "/*\n"
    " * To soften the diffuse contribution from local lights, the dot product from\n"
    " * the Lambertian model is scaled by 1/2, add 1/2 and squared.  The result is\n"
    " * that this dot product, which normally lies in the range of -1 to +1, is\n"
    " * instead in the range of 0 to 1 and has a more pleasing falloff.\n"
    " */\n"
    "//#define HALF_LAMBERT\n"
    "\n"
    "varying vec2 var_TexDiffuse;\n"
    "varying vec2 var_TexNormal;\n"
    "varying vec2 var_TexSpecular;\n"
    "varying vec4 var_TexLight;\n"
    "varying mediump vec4 var_Color;\n"
    "varying vec3 var_L;\n"
    "#if defined(BLINN_PHONG)\n"
    "varying vec3 var_H;\n"
    "#else\n"
    "varying vec3 var_V;\n"
    "#endif\n"
    "\n"
    "uniform vec4 u_diffuseColor;\n"
    "uniform vec4 u_specularColor;\n"
    "//uniform float u_specularExponent;\n"
    "\n"
    "uniform sampler2D u_fragmentMap0;\t/* u_bumpTexture */\n"
    "uniform sampler2D u_fragmentMap1;\t/* u_lightFalloffTexture */\n"
    "uniform sampler2D u_fragmentMap2;\t/* u_lightProjectionTexture */\n"
    "uniform sampler2D u_fragmentMap3;\t/* u_diffuseTexture */\n"
    "uniform sampler2D u_fragmentMap4;\t/* u_specularTexture */\n"
    "uniform sampler2D u_fragmentMap5;\t/* u_specularFalloffTexture */\n"
    "\n"
    "void main(void)\n"
    "{\n"
    "\tfloat u_specularExponent = 4.0;\n"
    "\n"
    "\tvec3 L = normalize(var_L);\n"
    "#if defined(BLINN_PHONG)\n"
    "\tvec3 H = normalize(var_H);\n"
    "\tvec3 N = 2.0 * texture2D(u_fragmentMap0, var_TexNormal.st).agb - 1.0;\n"
    "#else\n"
    "\tvec3 V = normalize(var_V);\n"
    "\tvec3 N = normalize(2.0 * texture2D(u_fragmentMap0, var_TexNormal.st).agb - 1.0);\n"
    "#endif\n"
    "\n"
    "\tfloat NdotL = clamp(dot(N, L), 0.0, 1.0);\n"
    "#if defined(HALF_LAMBERT)\n"
    "\tNdotL *= 0.5;\n"
    "\tNdotL += 0.5;\n"
    "\tNdotL = NdotL * NdotL;\n"
    "#endif\n"
    "#if defined(BLINN_PHONG)\n"
    "\tfloat NdotH = clamp(dot(N, H), 0.0, 1.0);\n"
    "#endif\n"
    "\n"
    "\tvec3 lightProjection = texture2DProj(u_fragmentMap2, var_TexLight.xyw).rgb;\n"
    "\tvec3 lightFalloff = texture2D(u_fragmentMap1, vec2(var_TexLight.z, 0.5)).rgb;\n"
    "\tvec3 diffuseColor = texture2D(u_fragmentMap3, var_TexDiffuse).rgb * u_diffuseColor.rgb;\n"
    "\tvec3 specularColor = 2.0 * texture2D(u_fragmentMap4, var_TexSpecular).rgb * u_specularColor.rgb;\n"
    "\n"
    "#if defined(BLINN_PHONG)\n"
    "\tfloat specularFalloff = pow(NdotH, u_specularExponent);\n"
    "#else\n"
    "\tvec3 R = -reflect(L, N);\n"
    "\tfloat RdotV = clamp(dot(R, V), 0.0, 1.0);\n"
    "\tfloat specularFalloff = pow(RdotV, u_specularExponent);\n"
    "#endif\n"
    "\n"
    "\tvec3 color;\n"
    "\tcolor = diffuseColor;\n"
    "\tcolor += specularFalloff * specularColor;\n"
    "\tcolor *= NdotL * lightProjection;\n"
    "\tcolor *= lightFalloff;\n"
    "\n"
    "\tgl_FragColor = vec4(color, 1.0) * var_Color;\n"
    "}\n";

static const char* const fogShaderVP = "\n";
static const char* const fogShaderFP = "\n";

shaderProgram_t interactionShader;
shaderProgram_t fogShader;
//shaderProgram_t zfillShader;
//shaderProgram_t stencilShadowShader;
//shaderProgram_t defaultShader;

/*
====================
GL_UseProgram
====================
*/
static void GL_UseProgram(shaderProgram_t *program)
{
  if (backEnd.glState.currentProgram == program) {
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
static void GL_Uniform1fv(GLint location, const GLfloat *value)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_Uniform1fv: no current program object\n");
    return;
  }

  qglUniform1fv(*(GLint *)((char *)backEnd.glState.currentProgram + location), 1, value);
}

/*
====================
GL_Uniform4fv
====================
*/
static void GL_Uniform4fv(GLint location, const GLfloat *value)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_Uniform4fv: no current program object\n");
    return;
  }

  qglUniform4fv(*(GLint *)((char *)backEnd.glState.currentProgram + location), 1, value);
}

/*
====================
GL_UniformMatrix4fv
====================
*/
static void GL_UniformMatrix4fv(GLint location, const GLfloat *value)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_Uniform4fv: no current program object\n");
    return;
  }

  qglUniformMatrix4fv(*(GLint *)((char *)backEnd.glState.currentProgram + location), 1, GL_FALSE, value);
}

/*
====================
GL_EnableVertexAttribArray
====================
*/
static void GL_EnableVertexAttribArray(GLuint index)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_EnableVertexAttribArray: no current program object\n");
    return;
  }

  if ((*(GLint *)((char *)backEnd.glState.currentProgram + index)) == -1) {
    common->Printf("GL_EnableVertexAttribArray: unbound attribute index\n");
    return;
  }

  qglEnableVertexAttribArray(*(GLint *)((char *)backEnd.glState.currentProgram + index));
}

/*
====================
GL_DisableVertexAttribArray
====================
*/
static void GL_DisableVertexAttribArray(GLuint index)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_DisableVertexAttribArray: no current program object\n");
    return;
  }

  if ((*(GLint *)((char *)backEnd.glState.currentProgram + index)) == -1) {
    common->Printf("GL_DisableVertexAttribArray: unbound attribute index\n");
    return;
  }

  qglDisableVertexAttribArray(*(GLint *)((char *)backEnd.glState.currentProgram + index));
}

/*
====================
GL_VertexAttribPointer
====================
*/
static void GL_VertexAttribPointer(GLuint index, GLint size, GLenum type,
                            GLboolean normalized, GLsizei stride,
                            const GLvoid *pointer)
{
  if (!backEnd.glState.currentProgram) {
    common->Printf("GL_VertexAttribPointer: no current program object\n");
    return;
  }

  if ((*(GLint *)((char *)backEnd.glState.currentProgram + index)) == -1) {
    common->Printf("GL_VertexAttribPointer: unbound attribute index\n");
    return;
  }

  qglVertexAttribPointer(*(GLint *)((char *)backEnd.glState.currentProgram + index),
                         size, type, normalized, stride, pointer);
}

/*
=================
R_LoadGLSLShader

loads GLSL vertex or fragment shaders
=================
*/
static void R_LoadGLSLShader(const char *buffer, shaderProgram_t *shaderProgram, GLenum type) {
  if (!glConfig.isInitialized) {
    return;
  }

  switch (type) {
    case GL_VERTEX_SHADER:
      // create vertex shader
      shaderProgram->vertexShader = qglCreateShader(GL_VERTEX_SHADER);
      qglShaderSource(shaderProgram->vertexShader, 1, (const GLchar **) &buffer, 0);
      qglCompileShader(shaderProgram->vertexShader);
      break;
    case GL_FRAGMENT_SHADER:
      // create fragment shader
      shaderProgram->fragmentShader = qglCreateShader(GL_FRAGMENT_SHADER);
      qglShaderSource(shaderProgram->fragmentShader, 1, (const GLchar **) &buffer, 0);
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
static bool R_LinkGLSLShader(shaderProgram_t *shaderProgram, bool needsAttributes) {
  char buf[BUFSIZ];
  int len;
  GLint status;
  GLint linked;

  shaderProgram->program = qglCreateProgram();

  qglAttachShader(shaderProgram->program, shaderProgram->vertexShader);
  qglAttachShader(shaderProgram->program, shaderProgram->fragmentShader);

  // Always prebind attribute 0, which is a mandatory requirement for WebGL
  // Let the rest be decided by GL
  qglBindAttribLocation(shaderProgram->program, 0, "attr_Vertex");

  qglLinkProgram(shaderProgram->program);

  qglGetProgramiv(shaderProgram->program, GL_LINK_STATUS, &linked);

  if (com_developer.GetBool()) {
    qglGetShaderInfoLog(shaderProgram->vertexShader, sizeof(buf), &len, buf);
    common->Printf("VS:\n%.*s\n", len, buf);
    qglGetShaderInfoLog(shaderProgram->fragmentShader, sizeof(buf), &len, buf);
    common->Printf("FS:\n%.*s\n", len, buf);
  }

  if (!linked) {
    common->Error("R_LinkGLSLShader: program failed to link\n");
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
static bool R_ValidateGLSLProgram(shaderProgram_t *shaderProgram) {
  GLint validProgram;

  qglValidateProgram(shaderProgram->program);

  qglGetProgramiv(shaderProgram->program, GL_VALIDATE_STATUS, &validProgram);

  if (!validProgram) {
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
static void RB_GLSL_GetUniformLocations(shaderProgram_t *shader) {
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
  shader->diffuseColor = qglGetUniformLocation(shader->program, "u_diffuseColor");
  shader->specularColor = qglGetUniformLocation(shader->program, "u_specularColor");
  shader->glColor = qglGetUniformLocation(shader->program, "u_glColor");
  shader->alphaTest = qglGetUniformLocation(shader->program, "u_alphaTest");
  shader->specularExponent = qglGetUniformLocation(shader->program, "u_specularExponent");

  shader->eyeOrigin = qglGetUniformLocation(shader->program, "u_eyeOrigin");
  shader->localEyeOrigin = qglGetUniformLocation(shader->program, "u_localEyeOrigin");
  shader->nonPowerOfTwo = qglGetUniformLocation(shader->program, "u_nonPowerOfTwo");
  shader->windowCoords = qglGetUniformLocation(shader->program, "u_windowCoords");

#ifdef USEREGAL
  shader->modelViewMatrix = qglGetUniformLocation(shader->program, "u_modelViewMatrix");
  shader->projectionMatrix = qglGetUniformLocation(shader->program, "u_projectionMatrix");
#else
  shader->modelViewProjectionMatrix = qglGetUniformLocation(shader->program, "u_modelViewProjectionMatrix");
#endif
  shader->textureMatrix = qglGetUniformLocation(shader->program, "u_textureMatrix");

  shader->attr_TexCoord = qglGetAttribLocation(shader->program, "attr_TexCoord");
  shader->attr_Tangent = qglGetAttribLocation(shader->program, "attr_Tangent");
  shader->attr_Bitangent = qglGetAttribLocation(shader->program, "attr_Bitangent");
  shader->attr_Normal = qglGetAttribLocation(shader->program, "attr_Normal");
  shader->attr_Vertex = qglGetAttribLocation(shader->program, "attr_Vertex");
  shader->attr_Color = qglGetAttribLocation(shader->program, "attr_Color");

  for (i = 0; i < MAX_VERTEX_PARMS; i++) {
    idStr::snPrintf(buffer, sizeof(buffer), "u_vertexParm%d", i);
    shader->u_vertexParm[i] = qglGetAttribLocation(shader->program, buffer);
  }

  for (i = 0; i < MAX_FRAGMENT_IMAGES; i++) {
    idStr::snPrintf(buffer, sizeof(buffer), "u_fragmentMap%d", i);
    shader->u_fragmentMap[i] = qglGetUniformLocation(shader->program, buffer);
    qglUniform1i(shader->u_fragmentMap[i], i);
  }

  GL_CheckErrors();

  GL_UseProgram(NULL);
}

/*
=================
RB_GLSL_InitShaders

=================
*/
static bool RB_GLSL_InitShaders(void) {
  memset(&interactionShader, 0, sizeof(shaderProgram_t));

  // load interation shaders
  R_LoadGLSLShader(interactionShaderVP, &interactionShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader(interactionShaderFP, &interactionShader, GL_FRAGMENT_SHADER);

  if (!R_LinkGLSLShader(&interactionShader, true) && !R_ValidateGLSLProgram(&interactionShader)) {
    return false;
  } else {
    RB_GLSL_GetUniformLocations(&interactionShader);
  }

  memset(&fogShader, 0, sizeof(shaderProgram_t));

  // load fog shaders
  //R_LoadGLSLShader(fogShaderVP, &fogShader, GL_VERTEX_SHADER);
  //R_LoadGLSLShader(fogShaderFP, &fogShader, GL_FRAGMENT_SHADER);

  //if (!R_LinkGLSLShader(&fogShader, true) && !R_ValidateGLSLProgram(&fogShader)) {
  //return false;
  //} else {
    //RB_GLSL_GetUniformLocations(&fogShader);
  //}

  return true;
}

/*
==================
R_ReloadGLSLPrograms_f
==================
*/
void R_ReloadGLSLPrograms_f(const idCmdArgs &args) {
  int i;

  common->Printf("----- R_ReloadGLSLPrograms -----\n");

  if (!RB_GLSL_InitShaders()) {
    common->Printf("GLSL shaders failed to init.\n");
  }

  common->Printf("-------------------------------\n");
}

/*
===============
RB_EnterWeaponDepthHack
===============
*/
static void RB_GLSL_EnterWeaponDepthHack(const drawSurf_t *surf) {
  qglDepthRange(0, 0.5);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] *= 0.25;

#ifdef USEREGAL
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), matrix);
#else
  float	mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, matrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
#endif
}

/*
===============
RB_EnterModelDepthHack
===============
*/
static void RB_GLSL_EnterModelDepthHack(const drawSurf_t *surf) {
  qglDepthRange(0.0f, 1.0f);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] -= surf->space->modelDepthHack;

#ifdef USEREGAL
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), matrix);
#else
  float	mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, matrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
#endif
}

/*
===============
RB_LeaveDepthHack
===============
*/
static void RB_GLSL_LeaveDepthHack(const drawSurf_t *surf) {
  qglDepthRange(0, 1);

#ifdef USEREGAL
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), backEnd.viewDef->projectionMatrix);
#else
  float	mat[16];
  myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
#endif
}

/*
====================
GL_SelectTextureNoClient
====================
*/
static void GL_SelectTextureNoClient(int unit) {
  backEnd.glState.currenttmu = unit;
  qglActiveTextureARB(GL_TEXTURE0 + unit);
}

/*
==================
RB_GLSL_DrawInteraction
==================
*/
static void RB_GLSL_DrawInteraction(const drawInteraction_t *din) {
  static const float zero[4] = {0, 0, 0, 0};
  static const float one[4] = {1, 1, 1, 1};
  static const float negOne[4] = {-1, -1, -1, -1};

  // load all the vertex program parameters
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
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

  switch (din->vertexColor) {
    case SVC_IGNORE:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), zero);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
      break;
    case SVC_MODULATE:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), one);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), zero);
      break;
    case SVC_INVERSE_MODULATE:
      GL_Uniform4fv(offsetof(shaderProgram_t, colorModulate), negOne);
      GL_Uniform4fv(offsetof(shaderProgram_t, colorAdd), one);
      break;
  }

  // set the constant colors
  GL_Uniform4fv(offsetof(shaderProgram_t, diffuseColor), din->diffuseColor.ToFloatPtr());
  GL_Uniform4fv(offsetof(shaderProgram_t, specularColor), din->specularColor.ToFloatPtr());

  // material may be NULL for shadow volumes
  float f;
  switch (din->surf->material->GetSurfaceType()) {
    case SURFTYPE_METAL:
    case SURFTYPE_RICOCHET:
      f = 4.0f;
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
      f = 4.0f;
      break;
  }
  GL_Uniform1fv(offsetof(shaderProgram_t, specularExponent), &f);

  // set the textures

  // texture 0 will be the per-surface bump map
  GL_SelectTextureNoClient(0);
  din->bumpImage->Bind();

  // texture 1 will be the light falloff texture
  GL_SelectTextureNoClient(1);
  din->lightFalloffImage->Bind();

  // texture 2 will be the light projection texture
  GL_SelectTextureNoClient(2);
  din->lightImage->Bind();

  // texture 3 is the per-surface diffuse map
  GL_SelectTextureNoClient(3);
  din->diffuseImage->Bind();

  // texture 4 is the per-surface specular map
  GL_SelectTextureNoClient(4);
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
static void RB_GLSL_CreateSingleDrawInteractions(const drawSurf_t *surf, void (*DrawInteraction)(const drawInteraction_t *)) {
  const idMaterial *surfaceShader = surf->material;
  const float *surfaceRegs = surf->shaderRegisters;
  const viewLight_t *vLight = backEnd.vLight;
  const idMaterial *lightShader = vLight->lightShader;
  const float *lightRegs = vLight->shaderRegisters;
  drawInteraction_t inter;

  if (r_skipInteractions.GetBool() || !surf->geo || !surf->geo->ambientCache) {
    return;
  }

  // change the scissor if needed
  if (r_useScissor.GetBool() && !backEnd.currentScissor.Equals(surf->scissorRect)) {
    backEnd.currentScissor = surf->scissorRect;
    qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
               backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
               backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
               backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
  }

  // hack depth range if needed
  if (surf->space->weaponDepthHack) {
    RB_GLSL_EnterWeaponDepthHack(surf);
  }

  if (surf->space->modelDepthHack) {
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

  for (int i = 0; i < 4; i++) {
    R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i]);
  }

  for (int lightStageNum = 0; lightStageNum < lightShader->GetNumStages(); lightStageNum++) {
    const shaderStage_t *lightStage = lightShader->GetStage(lightStageNum);

    // ignore stages that fail the condition
    if (!lightRegs[lightStage->conditionRegister]) {
      continue;
    }

    inter.lightImage = lightStage->texture.image;

    memcpy(inter.lightProjection, lightProject, sizeof(inter.lightProjection));

    // now multiply the texgen by the light texture matrix
    if (lightStage->texture.hasMatrix) {
      RB_GetShaderTextureMatrix(lightRegs, &lightStage->texture, backEnd.lightTextureMatrix);
      RB_BakeTextureMatrixIntoTexgen(reinterpret_cast<class idPlane *>(inter.lightProjection), NULL);
    }

    inter.bumpImage = NULL;
    inter.specularImage = NULL;
    inter.diffuseImage = NULL;
    inter.diffuseColor[0] = inter.diffuseColor[1] = inter.diffuseColor[2] = inter.diffuseColor[3] = 0;
    inter.specularColor[0] = inter.specularColor[1] = inter.specularColor[2] = inter.specularColor[3] = 0;

    float lightColor[4];

    // backEnd.lightScale is calculated so that lightColor[] will never exceed
    // tr.backEndRendererMaxLight
    lightColor[0] = backEnd.lightScale * lightRegs[lightStage->color.registers[0]];
    lightColor[1] = backEnd.lightScale * lightRegs[lightStage->color.registers[1]];
    lightColor[2] = backEnd.lightScale * lightRegs[lightStage->color.registers[2]];
    lightColor[3] = lightRegs[lightStage->color.registers[3]];

    // go through the individual stages
    for (int surfaceStageNum = 0; surfaceStageNum < surfaceShader->GetNumStages(); surfaceStageNum++) {
      const shaderStage_t *surfaceStage = surfaceShader->GetStage(surfaceStageNum);

      switch (surfaceStage->lighting) {
        case SL_AMBIENT: {
          // ignore ambient stages while drawing interactions
          break;
        }
        case SL_BUMP: {
          // ignore stage that fails the condition
          if (!surfaceRegs[surfaceStage->conditionRegister]) {
            break;
          }

          // draw any previous interaction
          RB_SubmittInteraction(&inter, DrawInteraction);
          inter.diffuseImage = NULL;
          inter.specularImage = NULL;
          R_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.bumpImage, inter.bumpMatrix, NULL);
          break;
        }
        case SL_DIFFUSE: {
          // ignore stage that fails the condition
          if (!surfaceRegs[surfaceStage->conditionRegister]) {
            break;
          }

          if (inter.diffuseImage) {
            RB_SubmittInteraction(&inter, DrawInteraction);
          }

          R_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.diffuseImage,
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
          if (!surfaceRegs[surfaceStage->conditionRegister]) {
            break;
          }

          if (inter.specularImage) {
            RB_SubmittInteraction(&inter, DrawInteraction);
          }

          R_SetDrawInteraction(surfaceStage, surfaceRegs, &inter.specularImage,
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
  if (surf->space->weaponDepthHack || surf->space->modelDepthHack != 0.0f) {
    RB_GLSL_LeaveDepthHack(surf);
  }
}

/*
=============
RB_GLSL_CreateDrawInteractions

=============
*/
static void RB_GLSL_CreateDrawInteractions(const drawSurf_t *surf) {
  if (!surf) {
    GL_UseProgram(NULL);
    return;
  }

  // perform setup here that will be constant for all interactions
  GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc);

  // bind the vertex and fragment shader
  GL_UseProgram(&interactionShader);

  // enable the vertex arrays
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Tangent));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Bitangent));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Vertex));  // gl_Vertex
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));  // gl_Color

  // texture 5 is the specular lookup table
  GL_SelectTextureNoClient(5);
  globalImages->specularTableImage->Bind();

#ifdef USEREGAL
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), backEnd.viewDef->projectionMatrix);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), mat4_identity.ToFloatPtr());
#else
  float   mat[16];
  myGlMultMatrix(mat4_identity.ToFloatPtr(), backEnd.viewDef->projectionMatrix, mat);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
#endif

  for (; surf; surf = surf->nextOnLight) {
    // perform setup here that will not change over multiple interaction passes

#ifdef USEREGAL
    // set the modelview matrix for the viewer
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), surf->space->modelViewMatrix);
#else
    float   mat[16];
    myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewProjectionMatrix), mat);
#endif

    // set the vertex pointers
    idDrawVert *ac = (idDrawVert *) vertexCache.Position(surf->geo->ambientCache);

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
                           ac->color);

    // this may cause RB_GLSL_DrawInteraction to be exacuted multiple
    // times with different colors and images if the surface or light have multiple layers
    RB_GLSL_CreateSingleDrawInteractions(surf, RB_GLSL_DrawInteraction);
  }

  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Tangent));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Bitangent));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Normal));
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Vertex));  // gl_Vertex
  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Color));  // gl_Color

  // disable features
  GL_SelectTextureNoClient(5);
  globalImages->BindNull();

  GL_SelectTextureNoClient(4);
  globalImages->BindNull();

  GL_SelectTextureNoClient(3);
  globalImages->BindNull();

  GL_SelectTextureNoClient(2);
  globalImages->BindNull();

  GL_SelectTextureNoClient(1);
  globalImages->BindNull();

  backEnd.glState.currenttmu = -1;
  GL_SelectTexture(0);

  GL_UseProgram(NULL);

  // Restore fixed function pipeline to an acceptable state
  qglEnableClientState(GL_VERTEX_ARRAY);
  qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
  qglDisableClientState(GL_COLOR_ARRAY);
  qglDisableClientState(GL_NORMAL_ARRAY);
}

/*
==================
RB_GLSL_DrawInteractions
==================
*/
void RB_GLSL_DrawInteractions(void) {
  viewLight_t *vLight;
  const idMaterial *lightShader;

  GL_SelectTexture(0);

  //
  // for each light, perform adding and shadowing
  //
  for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {
    backEnd.vLight = vLight;

    // do fogging later
    if (vLight->lightShader->IsFogLight()) {
      continue;
    }

    if (vLight->lightShader->IsBlendLight()) {
      continue;
    }

    if (!vLight->localInteractions && !vLight->globalInteractions
        && !vLight->translucentInteractions) {
      continue;
    }

    lightShader = vLight->lightShader;

    // clear the stencil buffer if needed
    if (vLight->globalShadows || vLight->localShadows) {
      backEnd.currentScissor = vLight->scissorRect;

      if (r_useScissor.GetBool()) {
        qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                   backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                   backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                   backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
      }

      qglClear(GL_STENCIL_BUFFER_BIT);
    } else {
      // no shadows, so no need to read or write the stencil buffer
      // we might in theory want to use GL_ALWAYS instead of disabling
      // completely, to satisfy the invarience rules
      qglStencilFunc(GL_ALWAYS, 128, 255);
    }

    RB_StencilShadowPass(vLight->globalShadows);                    // Caution: Fixed Function Pipeline
    RB_GLSL_CreateDrawInteractions(vLight->localInteractions);
    RB_StencilShadowPass(vLight->localShadows);                      // Caution: Fixed Function Pipeline
    RB_GLSL_CreateDrawInteractions(vLight->globalInteractions);

    // translucent surfaces never get stencil shadowed
    if (r_skipTranslucent.GetBool()) {
      continue;
    }

    qglStencilFunc(GL_ALWAYS, 128, 255);
    backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
    RB_GLSL_CreateDrawInteractions(vLight->translucentInteractions);

    backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
  }

  // disable stencil shadow test
  qglStencilFunc(GL_ALWAYS, 128, 255);

  GL_SelectTexture(0);

  // Restore fixed function pipeline to an acceptable state
  qglEnableClientState(GL_VERTEX_ARRAY);
  qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
  qglDisableClientState(GL_COLOR_ARRAY);
  qglDisableClientState(GL_NORMAL_ARRAY);
}
