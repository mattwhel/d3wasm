#version 100

precision mediump float;

varying vec4 var_Color;

void main(void)
{
	gl_FragColor = var_Color;
}
