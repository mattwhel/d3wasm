#version 100
precision mediump float;

// In
attribute highp vec4 attr_Vertex;

// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform lowp vec4 u_glColor;

// Out
// gl_Position
varying lowp vec4 var_Color;

void main(void)
{
	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;

	var_Color = u_glColor;
}
