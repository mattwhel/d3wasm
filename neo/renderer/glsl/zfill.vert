#version 100

precision mediump float;

attribute vec4 attr_Dummy;
attribute vec4 attr_TexCoord;
attribute highp vec4 attr_Vertex;

uniform highp mat4 u_modelViewProjectionMatrix;

varying vec2 var_TexDiffuse;

void main(void)
{
	var_TexDiffuse = attr_TexCoord.xy;

	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
