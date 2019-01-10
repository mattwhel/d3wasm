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

shaderProgram_t interactionShader;
shaderProgram_t zfillShader;
shaderProgram_t stencilShadowShader;
shaderProgram_t defaultShader;

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
static void R_LoadGLSLShader(const char *name, shaderProgram_t *shaderProgram, GLenum type) {
  idStr fullPath = "glsl/";
  fullPath += name;
  char *fileBuffer;
  char *buffer;

  common->Printf("%s", fullPath.c_str());

  // load the program even if we don't support it, so
  // fs_copyfiles can generate cross-platform data dumps
  fileSystem->ReadFile(fullPath.c_str(), (void **) &fileBuffer, NULL);

  if (!fileBuffer) {
    common->Printf(": File not found\n");
    return;
  }

  // copy to stack memory and free
  buffer = (char *) _alloca(strlen(fileBuffer) + 1);
  strcpy(buffer, fileBuffer);
  fileSystem->FreeFile(fileBuffer);

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

  common->Printf("\n");
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

  shader->modelViewMatrix = qglGetUniformLocation(shader->program, "u_modelViewMatrix");
  shader->projectionMatrix = qglGetUniformLocation(shader->program, "u_projectionMatrix");
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
  R_LoadGLSLShader("interaction.vert", &interactionShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader("interaction.frag", &interactionShader, GL_FRAGMENT_SHADER);

  if (!R_LinkGLSLShader(&interactionShader, true) && !R_ValidateGLSLProgram(&interactionShader)) {
    return false;
  } else {
    RB_GLSL_GetUniformLocations(&interactionShader);
  }

  memset(&zfillShader, 0, sizeof(shaderProgram_t));

  // load zfill shaders
  R_LoadGLSLShader("zfill.vert", &zfillShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader("zfill.frag", &zfillShader, GL_FRAGMENT_SHADER);

  if (!R_LinkGLSLShader(&zfillShader, true) && !R_ValidateGLSLProgram(&zfillShader)) {
    return false;
  } else {
    RB_GLSL_GetUniformLocations(&zfillShader);
  }

  /*memset(&defaultShader, 0, sizeof(shaderProgram_t));

  // load interation shaders
  R_LoadGLSLShader("default.vert", &defaultShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader("default.frag", &defaultShader, GL_FRAGMENT_SHADER);

  if (!R_LinkGLSLShader(&defaultShader, true) && !R_ValidateGLSLProgram(&defaultShader)) {
    return false;
  } else {
    RB_GLSL_GetUniformLocations(&defaultShader);
  }

  memset(&stencilShadowShader, 0, sizeof(shaderProgram_t));

  // load interation shaders
  R_LoadGLSLShader("shadow.vert", &stencilShadowShader, GL_VERTEX_SHADER);
  R_LoadGLSLShader("shadow.frag", &stencilShadowShader, GL_FRAGMENT_SHADER);

  if (!R_LinkGLSLShader(&stencilShadowShader, true) && !R_ValidateGLSLProgram(&stencilShadowShader)) {
    return false;
  } else {
    RB_GLSL_GetUniformLocations(&stencilShadowShader);
  }*/

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
  glDepthRangef(0, 0.5);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] *= 0.25;

  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), matrix);
}

/*
===============
RB_EnterModelDepthHack
===============
*/
static void RB_GLSL_EnterModelDepthHack(const drawSurf_t *surf) {
  glDepthRangef(0.0f, 1.0f);

  float matrix[16];
  memcpy(matrix, backEnd.viewDef->projectionMatrix, sizeof(matrix));
  matrix[14] -= surf->space->modelDepthHack;

  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), matrix);
}

/*
===============
RB_LeaveDepthHack
===============
*/
static void RB_GLSL_LeaveDepthHack(const drawSurf_t *surf) {
  glDepthRangef(0, 1);

  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), backEnd.viewDef->projectionMatrix);
}

/*
====================
RB_RenderDrawSurfListWithFunction

The triangle functions can check backEnd.currentSpace != surf->space
to see if they need to perform any new matrix setup.  The modelview
matrix will already have been loaded, and backEnd.currentSpace will
be updated after the triangle function completes.
====================
*/
static void RB_GLSL_RenderDrawSurfListWithFunction(drawSurf_t **drawSurfs, int numDrawSurfs,
                                                   void (*triFunc_)(const drawSurf_t *)) {
  int i;
  const drawSurf_t *drawSurf;

  backEnd.currentSpace = NULL;

  for (i = 0; i < numDrawSurfs; i++) {
    drawSurf = drawSurfs[i];

    GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), drawSurf->space->modelViewMatrix);

    if (drawSurf->space->weaponDepthHack) {
      RB_GLSL_EnterWeaponDepthHack(drawSurf);
    }

    if (drawSurf->space->modelDepthHack != 0.0f) {
      RB_GLSL_EnterModelDepthHack(drawSurf);
    }

    // change the scissor if needed
    if (r_useScissor.GetBool() && !backEnd.currentScissor.Equals(drawSurf->scissorRect)) {
      backEnd.currentScissor = drawSurf->scissorRect;
      qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                 backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                 backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                 backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
    }

    // render it
    triFunc_(drawSurf);

    if (drawSurf->space->weaponDepthHack || drawSurf->space->modelDepthHack != 0.0f) {
      RB_GLSL_LeaveDepthHack(drawSurf);
    }

    backEnd.currentSpace = drawSurf->space;
  }
}

/*
======================
RB_LoadShaderTextureMatrix
======================
*/
static void RB_GLSL_LoadShaderTextureMatrix(const float *shaderRegisters, const textureStage_t *texture)
{
  if (texture->hasMatrix) {
    float	matrix[16];

    RB_GetShaderTextureMatrix(shaderRegisters, texture, matrix);
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), matrix);
  } else {
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
  }
}

/*
================
RB_PrepareStageTexturing
================
*/
void RB_GLSL_PrepareStageTexturing(const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac) {
  // set privatePolygonOffset if necessary
  if (pStage->privatePolygonOffset) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset);
  }

  // set the texture matrix if needed
  RB_LoadShaderTextureMatrix(surf->shaderRegisters, &pStage->texture);

  // texgens
  if (pStage->texture.texgen == TG_DIFFUSE_CUBE) {
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 3, GL_FLOAT, false, sizeof(idDrawVert),
                           ac->normal.ToFloatPtr());
  }
  else if (pStage->texture.texgen == TG_SKYBOX_CUBE || pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 3, GL_FLOAT, false, 0,
                           vertexCache.Position(surf->dynamicTexCoords));
  }
}

/*
================
RB_FinishStageTexturing
================
*/
void RB_GLSL_FinishStageTexturing(const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac) {
  // unset privatePolygonOffset if necessary
  if (pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  if (pStage->texture.texgen == TG_DIFFUSE_CUBE || pStage->texture.texgen == TG_SKYBOX_CUBE
      || pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
    GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 2, GL_FLOAT, false, sizeof(idDrawVert),
                           (void *) &ac->st);
  }

  if (pStage->texture.hasMatrix) {
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, textureMatrix), mat4_identity.ToFloatPtr());
  }
}

/*
==================
RB_T_FillDepthBuffer
==================
*/
static void RB_T_GLSL_FillDepthBuffer(const drawSurf_t *surf) {
  int stage;
  const idMaterial *shader;
  const shaderStage_t *pStage;
  const float *regs;
  float color[4];
  const srfTriangles_t *tri;
  const float one[1] = {1};

  tri = surf->geo;
  shader = surf->material;

#if 0
  // CLIP PLANES TODO
  // update the clip plane if needed
  if (backEnd.viewDef->numClipPlanes && surf->space != backEnd.currentSpace) {
    GL_SelectTexture(1);

    idPlane plane;

    R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.viewDef->clipPlanes[0], plane);
    plane[3] += 0.5;  // the notch is in the middle
    qglTexGenfv(GL_S, GL_OBJECT_PLANE, plane.ToFloatPtr());
    GL_SelectTexture(0);
  }
#endif

  if (!shader->IsDrawn()) {
    return;
  }

  // some deforms may disable themselves by setting numIndexes = 0
  if (!tri->numIndexes) {
    return;
  }

  // translucent surfaces don't put anything in the depth buffer and don't
  // test against it, which makes them fail the mirror clip plane operation
  if (shader->Coverage() == MC_TRANSLUCENT) {
    return;
  }

  if (!tri->ambientCache) {
    common->Printf("RB_T_FillDepthBuffer: !tri->ambientCache\n");
    return;
  }

  // get the expressions for conditionals / color / texcoords
  regs = surf->shaderRegisters;

  // if all stages of a material have been conditioned off, don't do anything
  for (stage = 0; stage < shader->GetNumStages(); stage++) {
    pStage = shader->GetStage(stage);
    // check the stage enable condition
    if (regs[pStage->conditionRegister] != 0) {
      break;
    }
  }
  if (stage == shader->GetNumStages()) {
    return;
  }

  // set polygon offset if necessary
  if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglEnable(GL_POLYGON_OFFSET_FILL);
    qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
  }

  // subviews will just down-modulate the color buffer by overbright
  if (shader->GetSort() == SS_SUBVIEW) {
    GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS);
    color[0] =
    color[1] =
    color[2] = (1.0 / backEnd.overBright);
    color[3] = 1;
  } else {
    // others just draw black
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
    color[3] = 1;
  }

  idDrawVert *ac = (idDrawVert *) vertexCache.Position(tri->ambientCache);
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_Vertex));
  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_Vertex), 3, GL_FLOAT, false, sizeof(idDrawVert),
                         ac->xyz.ToFloatPtr());
  //GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));   // Already done by caller function
  GL_VertexAttribPointer(offsetof(shaderProgram_t, attr_TexCoord), 2, GL_FLOAT, false, sizeof(idDrawVert),
                         reinterpret_cast<void *>(&ac->st));

  bool drawSolid = false;

  if (shader->Coverage() == MC_OPAQUE) {
    drawSolid = true;
  }

  // we may have multiple alpha tested stages
  if (shader->Coverage() == MC_PERFORATED) {
    // if the only alpha tested stages are condition register omitted,
    // draw a normal opaque surface
    bool didDraw = false;

    // perforated surfaces may have multiple alpha tested stages
    for (stage = 0; stage < shader->GetNumStages(); stage++) {
      pStage = shader->GetStage(stage);

      if (!pStage->hasAlphaTest) {
        continue;
      }

      // check the stage enable condition
      if (regs[pStage->conditionRegister] == 0) {
        continue;
      }

      // if we at least tried to draw an alpha tested stage,
      // we won't draw the opaque surface
      didDraw = true;

      // set the alpha modulate
      color[3] = regs[pStage->color.registers[3]];

      // skip the entire stage if alpha would be black
      if (color[3] <= 0) {
        continue;
      }
      GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);
      GL_Uniform1fv(offsetof(shaderProgram_t, alphaTest), &regs[pStage->alphaTestRegister]);

      // bind the texture
      pStage->texture.image->Bind();

      // set texture matrix and texGens
      RB_GLSL_PrepareStageTexturing(pStage, surf, ac);

      // draw it
      RB_DrawElementsWithCounters(tri);

      RB_GLSL_FinishStageTexturing(pStage, surf, ac);
    }

    if (!didDraw) {
      drawSolid = true;
    }
  }

  // draw the entire surface solid
  if (drawSolid) {
    GL_Uniform4fv(offsetof(shaderProgram_t, glColor), color);
    GL_Uniform1fv(offsetof(shaderProgram_t, alphaTest), one);
    globalImages->whiteImage->Bind();

    // draw it
    RB_DrawElementsWithCounters(tri);
  }

  // reset polygon offset
  if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    qglDisable(GL_POLYGON_OFFSET_FILL);
  }

  // reset blending
  if (shader->GetSort() == SS_SUBVIEW) {
    GL_State(GLS_DEPTHFUNC_LESS);
  }

  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_Vertex));
  //GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));    Already done by caller function
}

/*
=====================
RB_GLSL_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_GLSL_FillDepthBuffer(drawSurf_t **drawSurfs, int numDrawSurfs) {
  // if we are just doing 2D rendering, no need to fill the depth buffer
  if (!backEnd.viewDef->viewEntitys) {
    return;
  }

  GL_UseProgram(&zfillShader);

#if 0
  // CLIP PLANES: TODO
  // Previous STD code
  // enable the second texture for mirror plane clipping if needed
  if (backEnd.viewDef->numClipPlanes) {
    GL_SelectTexture(1);
    globalImages->alphaNotchImage->Bind();
    qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
    qglEnable(GL_TEXTURE_GEN_S);
    qglTexCoord2f(1, 0.5);
  }
#endif

  // the first texture will be used for alpha tested surfaces
  GL_SelectTexture(0);
  GL_EnableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));

  // decal surfaces may enable polygon offset
  qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat());

  GL_State(GLS_DEPTHFUNC_LESS);

  // Enable stencil test if we are going to be using it for shadows.
  // If we didn't do this, it would be legal behavior to get z fighting
  // from the ambient pass and the light passes.
  qglEnable(GL_STENCIL_TEST);
  qglStencilFunc(GL_ALWAYS, 1, 255);

  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), backEnd.viewDef->projectionMatrix);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), mat4_identity.ToFloatPtr());

  RB_GLSL_RenderDrawSurfListWithFunction(drawSurfs, numDrawSurfs, RB_T_GLSL_FillDepthBuffer);

#if 0
  // CLIP PLANES TODO
  // Previous STD code
  if (backEnd.viewDef->numClipPlanes) {
    GL_SelectTexture(1);
    globalImages->BindNull();
    qglDisable(GL_TEXTURE_GEN_S);
    GL_SelectTexture(0);
  }
#endif

  GL_DisableVertexAttribArray(offsetof(shaderProgram_t, attr_TexCoord));

  GL_UseProgram(NULL);

  // Restore fixed function pipeline to an acceptable state
  qglEnableClientState(GL_VERTEX_ARRAY);
  qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
  qglDisableClientState(GL_COLOR_ARRAY);
  qglDisableClientState(GL_NORMAL_ARRAY);
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
==================
R_SetDrawInteractions
==================
*/
static void R_SetDrawInteraction( const shaderStage_t *surfaceStage, const float *surfaceRegs,
                           idImage **image, idVec4 matrix[2], float color[4] ) {
  *image = surfaceStage->texture.image;
  if ( surfaceStage->texture.hasMatrix ) {
    matrix[0][0] = surfaceRegs[surfaceStage->texture.matrix[0][0]];
    matrix[0][1] = surfaceRegs[surfaceStage->texture.matrix[0][1]];
    matrix[0][2] = 0;
    matrix[0][3] = surfaceRegs[surfaceStage->texture.matrix[0][2]];

    matrix[1][0] = surfaceRegs[surfaceStage->texture.matrix[1][0]];
    matrix[1][1] = surfaceRegs[surfaceStage->texture.matrix[1][1]];
    matrix[1][2] = 0;
    matrix[1][3] = surfaceRegs[surfaceStage->texture.matrix[1][2]];

    // we attempt to keep scrolls from generating incredibly large texture values, but
    // center rotations and center scales can still generate offsets that need to be > 1
    if ( matrix[0][3] < -40 || matrix[0][3] > 40 ) {
      matrix[0][3] -= (int)matrix[0][3];
    }
    if ( matrix[1][3] < -40 || matrix[1][3] > 40 ) {
      matrix[1][3] -= (int)matrix[1][3];
    }
  } else {
    matrix[0][0] = 1;
    matrix[0][1] = 0;
    matrix[0][2] = 0;
    matrix[0][3] = 0;

    matrix[1][0] = 0;
    matrix[1][1] = 1;
    matrix[1][2] = 0;
    matrix[1][3] = 0;
  }

  if ( color ) {
    for ( int i = 0 ; i < 4 ; i++ ) {
      color[i] = surfaceRegs[surfaceStage->color.registers[i]];
      // clamp here, so card with greater range don't look different.
      // we could perform overbrighting like we do for lights, but
      // it doesn't currently look worth it.
      if ( color[i] < 0 ) {
        color[i] = 0;
      } else if ( color[i] > 1.0 ) {
        color[i] = 1.0;
      }
    }
  }
}

/*
=================
RB_SubmittInteraction
=================
*/
static void RB_SubmittInteraction( drawInteraction_t *din, void (*DrawInteraction)(const drawInteraction_t *) ) {
  if ( !din->bumpImage ) {
    return;
  }

  if ( !din->diffuseImage || r_skipDiffuse.GetBool() ) {
    din->diffuseImage = globalImages->blackImage;
  }
  if ( !din->specularImage || r_skipSpecular.GetBool() || din->ambientLight ) {
    din->specularImage = globalImages->blackImage;
  }
  if ( !din->bumpImage || r_skipBump.GetBool() ) {
    din->bumpImage = globalImages->flatNormalMap;
  }

  // if we wouldn't draw anything, don't call the Draw function
  if (
      ( ( din->diffuseColor[0] > 0 ||
          din->diffuseColor[1] > 0 ||
          din->diffuseColor[2] > 0 ) && din->diffuseImage != globalImages->blackImage )
      || ( ( din->specularColor[0] > 0 ||
             din->specularColor[1] > 0 ||
             din->specularColor[2] > 0 ) && din->specularImage != globalImages->blackImage ) ) {
    DrawInteraction( din );
  }
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

  GL_UniformMatrix4fv(offsetof(shaderProgram_t, projectionMatrix), backEnd.viewDef->projectionMatrix);
  GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), mat4_identity.ToFloatPtr());

  for (; surf; surf = surf->nextOnLight) {
    // perform setup here that will not change over multiple interaction passes

    // set the modelview matrix for the viewer
    GL_UniformMatrix4fv(offsetof(shaderProgram_t, modelViewMatrix), surf->space->modelViewMatrix);

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
