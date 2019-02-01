#version 100
precision mediump float;
  
// In
varying vec2 var_TexFog;            // input Fog TexCoord
varying vec2 var_TexFogEnter;       // input FogEnter TexCoord
  
// Uniforms
uniform sampler2D u_fragmentMap0;\t // Fog Image
uniform sampler2D u_fragmentMap1;\t // Fog Enter Image
uniform lowp vec4 u_fogColor;       // Fog Color
  
// Out
// gl_FragCoord                     // output Fragment color
  
void main(void)
{
  gl_FragColor = texture2D( u_fragmentMap0, var_TexFog ) * texture2D( u_fragmentMap1, var_TexFogEnter ) * vec4(u_fogColor.rgb, 1.0);
}
