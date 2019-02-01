#version 100
precision mediump float;
        
// In
attribute vec4 attr_TexCoord;
attribute highp vec4 attr_Vertex;
        
// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform mat4 u_textureMatrix;
uniform vec4 u_texGen0S;
        
// Out
// gl_Position
varying vec4 var_TexDiffuse;
varying vec2 var_TexClip;
        
void main(void)
{
  var_TexClip = vec2( dot( u_texGen0S, attr_Vertex), 0 );
        
  var_TexDiffuse = (u_textureMatrix * attr_TexCoord);
        
  gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
