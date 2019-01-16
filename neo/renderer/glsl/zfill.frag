#version 100
precision highp float;

// In
varying vec2 var_texDiffuse;
varying vec2 var_texClip;

// Uniforms
uniform sampler2D u_fragmentMap0;
uniform sampler2D u_fragmentMap1;
uniform float u_alphaTest;
uniform vec4 u_glColor;
uniform bool u_clip;

// Out
// gl_FragCoord

void main(void)
{
    if( !u_clip )
    {
        if (u_alphaTest > texture2D(u_fragmentMap0, var_texDiffuse).a) {
            discard;
        }
    }
    else {
       if (u_alphaTest > (texture2D(u_fragmentMap0, var_texDiffuse).a * texture2D(u_fragmentMap1, var_texClip).a) ) {
            discard;
       }
    }

	gl_FragColor = u_glColor;
}
