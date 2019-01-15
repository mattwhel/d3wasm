#version 100
precision highp float;

// In
varying vec2 var_TexFog;            // input Fog TexCoord
varying vec2 var_TexFogEnter;       // input FogEnter TexCoord

// Uniforms
uniform sampler2D u_FogImage;       // Fog Image
uniform sampler2D u_FogEnterImage;  // FogEnter Image
uniform vec4      u_FogColor;       // Fog Color

// Out
// gl_FragCoord                     // output Fragment color

void main(void)
{
  gl_FragColor = texture2D( u_FogImage, var_TCFog ) * texture2D( var_TCFogEnter, u_FogEnterImage ) * vec4(u_FogColor.rgb, 1.0);
}
