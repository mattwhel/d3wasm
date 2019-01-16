#version 100
precision highp float;

// In
attribute vec4 attr_Vertex;      // input Vertex Coordinates

// Uniforms
uniform mat4 u_modelViewMatrix;  // ModelView Matrix
uniform mat4 u_projectionMatrix; // Projection Matrix
uniform vec4 u_texGen0S;         // fogPlane 0
uniform vec4 u_texGen0T;         // fogPlane 1
uniform vec4 u_texGen1S;         // fogPlane 3 (not 2!)
uniform vec4 u_texGen1T;         // fogPlane 2

// Out
// gl_Position                   // output Vertex Coordinates
varying vec2 var_texFog;         // output Fog TexCoord
varying vec2 var_texFogEnter;    // output FogEnter TexCoord

void main(void)
{
  gl_Position       = u_projectionMatrix * u_modelViewMatrix * attr_Vertex;

  var_texFog.x      = dot(u_texGen0S, attr_Vertex);
  var_texFog.y      = dot(u_texGen0T, attr_Vertex);

  var_texFogEnter.x = dot(u_texGen1S, attr_Vertex);
  var_texFogEnter.y = dot(u_texGen1T, attr_Vertex);
}
