#version 100
precision highp float;

// In
varying vec4 var_Color;

// Out
// gl_FragColor

void main(void)
{
	gl_FragColor = var_Color;
}
