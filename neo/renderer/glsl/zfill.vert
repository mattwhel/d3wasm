#version 100
precision highp float;

// In
attribute vec4 attr_TexCoord;
attribute vec4 attr_Vertex;

// Uniforms
uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;

// Out
// gl_Position
varying vec2 var_texDiffuse;

void main(void)
{
	var_texDiffuse = attr_TexCoord.xy;

	gl_Position = u_projectionMatrix * u_modelViewMatrix * attr_Vertex;
}
