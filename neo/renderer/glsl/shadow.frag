#version 100

precision lowp float;

varying lowp vec4 var_Color;

void main(void)
{
	gl_FragColor = var_Color;
}
