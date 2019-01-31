/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

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

#include "renderer/tr_local.h"

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
void RB_BakeTextureMatrixIntoTexgen( idMat4 & lightProject, const float *textureMatrix ) {
	float	genMatrix[16];
	float	final[16];

	genMatrix[0] = lightProject[0][0];
	genMatrix[4] = lightProject[0][1];
	genMatrix[8] = lightProject[0][2];
	genMatrix[12] = lightProject[0][3];

	genMatrix[1] = lightProject[1][0];
	genMatrix[5] = lightProject[1][1];
	genMatrix[9] = lightProject[1][2];
	genMatrix[13] = lightProject[1][3];

	genMatrix[2] = 0;
	genMatrix[6] = 0;
	genMatrix[10] = 0;
	genMatrix[14] = 0;

	genMatrix[3] = lightProject[3][0];
	genMatrix[7] = lightProject[3][1];
	genMatrix[11] = lightProject[3][2];
	genMatrix[15] = lightProject[3][3];

	myGlMultMatrix( genMatrix, textureMatrix, final );

	lightProject[0][0] = final[0];
	lightProject[0][1] = final[4];
	lightProject[0][2] = final[8];
	lightProject[0][3] = final[12];

	lightProject[1][0] = final[1];
	lightProject[1][1] = final[5];
	lightProject[1][2] = final[9];
	lightProject[1][3] = final[13];
}

void RB_ComputeMVP( const drawSurf_t * const surf, float mvp[16] )
{
  // Get the projection matrix
  float localProjectionMatrix[16];
  memcpy(localProjectionMatrix, backEnd.viewDef->projectionMatrix, sizeof(localProjectionMatrix));

  // Quick and dirty hacks on the projection matrix
  if ( surf->space->weaponDepthHack ) {
    localProjectionMatrix[14] = backEnd.viewDef->projectionMatrix[14] * 0.25;
  }
  if ( surf->space->modelDepthHack != 0.0 ) {
    localProjectionMatrix[14] = backEnd.viewDef->projectionMatrix[14] - surf->space->modelDepthHack;
  }

  // precompute the MVP
  myGlMultMatrix(surf->space->modelViewMatrix, localProjectionMatrix, mvp);
}

/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
RB_T_BlendLight

=====================

static void RB_T_BlendLight(const drawSurf_t *surf) {
  const srfTriangles_t *tri;

  tri = surf->geo;

  if (backEnd.currentSpace != surf->space) {
    idPlane lightProject[4];
    int i;

    for (i = 0; i < 4; i++) {
      R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i]);
    }

    GL_SelectTexture(0);
    qglTexGenfv(GL_S, GL_OBJECT_PLANE, lightProject[0].ToFloatPtr());
    qglTexGenfv(GL_T, GL_OBJECT_PLANE, lightProject[1].ToFloatPtr());
    qglTexGenfv(GL_Q, GL_OBJECT_PLANE, lightProject[2].ToFloatPtr());

    GL_SelectTexture(1);
    qglTexGenfv(GL_S, GL_OBJECT_PLANE, lightProject[3].ToFloatPtr());
  }

  // this gets used for both blend lights and shadow draws
  if (tri->ambientCache) {
    idDrawVert *ac = (idDrawVert *) vertexCache.Position(tri->ambientCache);
    qglVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
  } else if (tri->shadowCache) {
    shadowCache_t *sc = (shadowCache_t *) vertexCache.Position(tri->shadowCache);
    qglVertexPointer(3, GL_FLOAT, sizeof(shadowCache_t), sc->xyz.ToFloatPtr());
  }

  RB_DrawElementsWithCounters(tri);
}
*/
/*
=====================
RB_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================

static void RB_BlendLight(const drawSurf_t *drawSurfs, const drawSurf_t *drawSurfs2) {
  const idMaterial *lightShader;
  const shaderStage_t *stage;
  int i;
  const float *regs;

  if (!drawSurfs) {
    return;
  }
  if (r_skipBlendLights.GetBool()) {
    return;
  }

  lightShader = backEnd.vLight->lightShader;
  regs = backEnd.vLight->shaderRegisters;

  // texture 1 will get the falloff texture
  GL_SelectTexture(1);
  qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
  qglEnable(GL_TEXTURE_GEN_S);
	qglTexCoord2f( 0, 0.5 );
  backEnd.vLight->falloffImage->Bind();

  // texture 0 will get the projected texture
  GL_SelectTexture(0);
  qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
  qglEnable(GL_TEXTURE_GEN_S);
  qglEnable(GL_TEXTURE_GEN_T);
  qglEnable(GL_TEXTURE_GEN_Q);

  for (i = 0; i < lightShader->GetNumStages(); i++) {
    stage = lightShader->GetStage(i);

    if (!regs[stage->conditionRegister]) {
      continue;
    }

    GL_State(GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL);

    GL_SelectTexture(0);
    stage->texture.image->Bind();

    if (stage->texture.hasMatrix) {
      RB_LoadShaderTextureMatrix(regs, &stage->texture);
    }

    // get the modulate values from the light, including alpha, unlike normal lights
    backEnd.lightColor[0] = regs[stage->color.registers[0]];
    backEnd.lightColor[1] = regs[stage->color.registers[1]];
    backEnd.lightColor[2] = regs[stage->color.registers[2]];
    backEnd.lightColor[3] = regs[stage->color.registers[3]];
    qglColor4fv(backEnd.lightColor);

    RB_RenderDrawSurfChainWithFunction(drawSurfs, RB_T_BlendLight);
    RB_RenderDrawSurfChainWithFunction(drawSurfs2, RB_T_BlendLight);

    if (stage->texture.hasMatrix) {
      GL_SelectTexture(0);
      qglMatrixMode(GL_TEXTURE);
      qglLoadIdentity();
      qglMatrixMode(GL_MODELVIEW);
    }
  }

  GL_SelectTexture(1);
  qglDisable(GL_TEXTURE_GEN_S);
  globalImages->BindNull();

  GL_SelectTexture(0);
  qglDisable(GL_TEXTURE_GEN_S);
  qglDisable(GL_TEXTURE_GEN_T);
  qglDisable(GL_TEXTURE_GEN_Q);
}
*/
//========================================================================
/*
==================
RB_FogAllLights
==================
*/
void RB_FogAllLights(void) {
  const viewLight_t *vLight;

  if (r_skipFogLights.GetBool() || backEnd.viewDef->isXraySubview /* dont fog in xray mode*/ ) {
    return;
  }

  qglDisable(GL_STENCIL_TEST);

  for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {

    if (!vLight->lightShader->IsFogLight() && !vLight->lightShader->IsBlendLight()) {
      continue;
    }

    if (vLight->lightShader->IsFogLight()) {
      RB_GLSL_FogPass(vLight->globalInteractions, vLight->localInteractions, vLight);
    } else if (vLight->lightShader->IsBlendLight()) {
      //RB_BlendLight(vLight->globalInteractions, vLight->localInteractions);
    }
    qglDisable(GL_STENCIL_TEST);
  }

  qglEnable(GL_STENCIL_TEST);
}

//=========================================================================================

/*
=============
RB_RenderView
=============
*/
void RB_RenderView(void) {

  drawSurf_t **drawSurfs = (drawSurf_t * *) & backEnd.viewDef->drawSurfs[0];
  const int numDrawSurfs = backEnd.viewDef->numDrawSurfs;

  // clear the z buffer, set the projection matrix, etc
  RB_BeginDrawingView();

  // fill the depth buffer and clear color buffer to black except on subviews
	RB_GLSL_FillDepthBuffer( drawSurfs, numDrawSurfs );

  // main light renderer
  RB_GLSL_DrawInteractions();

  // disable stencil shadow test
  qglStencilFunc(GL_ALWAYS, 128, 255);

  // now draw any non-light dependent shading passes
  const int processed = RB_GLSL_DrawShaderPasses(drawSurfs, numDrawSurfs);

  // fob and blend lights
  RB_FogAllLights();

  // now draw any post-processing effects using _currentRender
  if (processed < numDrawSurfs) {
    RB_GLSL_DrawShaderPasses(drawSurfs + processed, numDrawSurfs - processed);
  }
}
