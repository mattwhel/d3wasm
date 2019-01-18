#version 100
precision lowp float;

// In
varying lowp vec4 var_Color;

// Out
// gl_FragColor

void main(void)
{
	gl_FragColor = var_Color;
}
