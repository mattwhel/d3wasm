#version 100
precision highp float;

// In
attribute vec4 attr_Vertex;

// Uniforms
uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;
uniform vec4 u_glColor;

// Out
// gl_Position
varying vec4 var_Color;

void main(void)
{
	gl_Position = u_projectionMatrix * u_modelViewMatrix * attr_Vertex;

	var_Color = u_glColor;
}
