#version 100
precision highp float;

// In
attribute vec4 attr_Color;
attribute vec4 attr_TexCoord;
attribute vec4 attr_Vertex;

// Uniforms
uniform mat4 u_modelViewProjectionMatrix;
uniform mat4 u_textureMatrix;
uniform vec4 u_colorAdd;
uniform vec4 u_colorModulate;

// Out
// gl_Position
varying vec2 var_TexDiffuse;
varying vec4 var_Color;

void main(void)
{
	var_TexDiffuse = (attr_TexCoord * u_textureMatrix).xy;

	var_Color = (attr_Color / 255.0) * u_colorModulate + u_colorAdd;

	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
