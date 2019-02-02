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

const char * const zfillClipShaderVP = R"(
#version 100
precision mediump float;
        
// In
attribute vec4 attr_TexCoord;
attribute highp vec4 attr_Vertex;
        
// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform mat4 u_textureMatrix;
uniform vec4 u_texGen0S;
        
// Out
// gl_Position
varying vec4 var_TexDiffuse;
varying vec2 var_TexClip;
        
void main(void)
{
  var_TexClip = vec2( dot( u_texGen0S, attr_Vertex), 0 );
        
  var_TexDiffuse = (u_textureMatrix * attr_TexCoord);
        
  gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
)";
