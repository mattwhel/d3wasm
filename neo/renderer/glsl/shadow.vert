#version 100
precision highp float;

// In
attribute vec4 attr_Vertex;

// Uniforms
uniform mat4 u_modelViewProjectionMatrix;
uniform vec4 u_glColor;

// Out
// gl_Position
varying vec4 var_Color;

void main(void)
{
	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;

	var_Color = u_glColor;
}
