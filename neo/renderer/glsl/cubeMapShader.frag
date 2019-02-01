#version 100
precision mediump float;
  
uniform samplerCube u_fragmentCubeMap0;
uniform lowp vec4 u_glColor;
  
varying vec4 var_TexCoord;
varying lowp vec4 var_Color;
  
void main(void)
{
  gl_FragColor = textureCube(u_fragmentCubeMap0, var_TexCoord.xyz) * u_glColor * var_Color;
}
