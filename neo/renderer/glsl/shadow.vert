#version 100

precision highp float;

attribute vec4 attr_Vertex;

uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;
uniform mediump vec4 u_glColor;
uniform vec4 u_lightOrigin;

varying mediump vec4 var_Color;

void main(void)
{
	gl_Position = (u_projectionMatrix * u_modelViewMatrix) * (attr_Vertex.w * u_lightOrigin + attr_Vertex - u_lightOrigin);

	var_Color = u_glColor;
}
