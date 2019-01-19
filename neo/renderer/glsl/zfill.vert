#version 100
precision mediump float;

// In
attribute vec4 attr_TexCoord;
attribute highp vec4 attr_Vertex;

// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform mat4 u_textureMatrix;

// Out
// gl_Position
varying vec2 var_texDiffuse;

void main(void)
{
    var_texDiffuse = (attr_TexCoord * u_textureMatrix).xy;

	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
