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

const char * const blendLightShaderVP = R"(
#version 100
precision mediump float;

// In
attribute highp vec4 attr_Vertex;

// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform mat4 u_fogMatrix;

// Out
// gl_Position
varying vec4 var_TexCoord0;
varying vec2 var_TexCoord1;

void main(void)
{
  gl_Position = u_modelViewProjectionMatrix * attr_Vertex;

  var_TexCoord0.x = dot( u_fogMatrix[0], attr_Vertex );
  var_TexCoord0.y = dot( u_fogMatrix[1], attr_Vertex );
  var_TexCoord0.z = 0.0;
  var_TexCoord0.w = dot( u_fogMatrix[2], attr_Vertex );

  var_TexCoord1.x = dot( u_fogMatrix[3], attr_Vertex );
  var_TexCoord1.y = 0.5;
}
)";