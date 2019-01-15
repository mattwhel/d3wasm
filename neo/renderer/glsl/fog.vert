#version 100
precision highp float;

// In
attribute vec4 attr_Vertex;      // input Vertex Coordinates

// Uniforms
uniform mat4 u_modelViewMatrix;  // ModelView Matrix
uniform mat4 u_projectionMatrix; // Projection Matrix
uniform vec4 u_Texgen0S;         // fogPlane 0
uniform vec4 u_Texgen0T;         // fogPlane 1
uniform vec4 u_Texgen1S;         // fogPlane 3 (not 2!)
uniform vec4 u_Texgen1T;         // fogPlane 2

// Out
// gl_Position                   // output Vertex Coordinates
varying vec2 var_TexFog;         // output Fog TexCoord
varying vec2 var_TexFogEnter;    // output FogEnter TexCoord

void main(void)
{
  gl_Position       = u_projectionMatrix * u_modelViewMatrix * attr_Vertex;

  var_TexFog.x      = dot(u_Texgen0S, attr_Vertex);
  var_TexFog.y      = dot(u_Texgen0T, attr_Vertex);

  var_TexFogEnter.x = dot(u_Texgen1S, attr_Vertex);
  var_TexFogEnter.y = dot(u_Texgen1T, attr_Vertex);
}
