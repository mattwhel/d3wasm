#version 100

precision mediump float;

attribute vec4 attr_Dummy;
attribute lowp vec4 attr_Color;
attribute vec4 attr_TexCoord;
attribute highp vec4 attr_Vertex;

uniform highp mat4 u_modelViewProjectionMatrix;
uniform mat4 u_textureMatrix;
uniform lowp vec4 u_colorAdd;
uniform lowp vec4 u_colorModulate;

varying vec2 var_TexDiffuse;
varying lowp vec4 var_Color;

void main(void)
{
	// # glMatrixMode(GL_TEXTURE)
	var_TexDiffuse = (attr_TexCoord * u_textureMatrix).xy;

	// # generate the vertex color, which can be 1.0, color, or 1.0 - color
	var_Color = (attr_Color / 255.0) * u_colorModulate + u_colorAdd;

	//gl_Position = ftransform();
	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
