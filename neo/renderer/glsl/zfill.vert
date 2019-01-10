#version 100

precision highp float;

attribute vec4 attr_TexCoord;
attribute vec4 attr_Vertex;

uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;

varying vec2 var_TexDiffuse;

void main(void)
{
	var_TexDiffuse = attr_TexCoord.xy;

	gl_Position = (u_projectionMatrix * u_modelViewMatrix) * attr_Vertex;
}
