#version 100
precision highp float;

// In
varying vec2 var_texFog;            // input Fog TexCoord
varying vec2 var_texFogEnter;       // input FogEnter TexCoord

// Uniforms
uniform sampler2D u_fragmentMap0;   // Fog Image
uniform sampler2D u_fragmentMap1;   // Fog Enter Image
uniform vec4      u_fogColor;       // Fog Color

// Out
// gl_FragCoord                     // output Fragment color

void main(void)
{
  gl_FragColor = texture2D( u_fogImage, var_texFog ) * texture2D( u_fogEnterImage , var_texFogEnter ) * vec4(u_fogColor.rgb, 1.0);
}
