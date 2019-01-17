#version 100
precision highp float;

// In
attribute vec4 attr_TexCoord;
attribute vec4 attr_Vertex;

// Uniforms
uniform mat4 u_modelViewProjectionMatrix;
uniform bool u_clip;
uniform vec4 u_texGen0S;

// Out
// gl_Position
varying vec2 var_texDiffuse;
varying vec2 var_texClip;

void main(void)
{
    if ( u_clip ) {
        var_texClip = vec2( dot( u_texGen0S, attr_Vertex), 0 );
    }

    var_texDiffuse = attr_TexCoord.xy;

	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
