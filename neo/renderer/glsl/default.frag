#version 100

precision highp float;

uniform sampler2D u_fragmentMap0;
uniform mediump vec4 u_glColor;

varying vec2 var_TexDiffuse;
varying mediump vec4 var_Color;

void main(void)
{
	gl_FragColor = texture2D(u_fragmentMap0, var_TexDiffuse) * u_glColor * var_Color;
}
