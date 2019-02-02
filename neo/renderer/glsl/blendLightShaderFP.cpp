/*
 * This file is part of the D3wasm project (http://www.continuation-labs.com/projects/d3wasm)
 * Copyright (c) 2019 Gabriel Cuvillier.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "glsl_shaders.h"

const char * const blendLightShaderFP = R"(
#version 100
precision mediump float;

// In
varying vec4 var_TexCoord0;
varying vec2 var_TexCoord1;

// Uniforms
uniform sampler2D u_fragmentMap0;
uniform sampler2D u_fragmentMap1;
uniform lowp vec4 u_glColor;

// Out
// gl_FragColor

void main(void)
{
  result = textureProj( u_fragmentMap0, var_TexCoord0.xyw ) * texture2D( u_fragmentMap1, var_TexCoord1 ) * vec4(u_glColor.rgb, 1.0);
}
)";
