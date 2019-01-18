#version 100
precision mediump float;

// Option to use Blinn Phong instead of Gouraud
//#define BLINN_PHONG

// In
attribute vec4 attr_TexCoord;
attribute vec3 attr_Tangent;
attribute vec3 attr_Bitangent;
attribute vec3 attr_Normal;
attribute highp vec4 attr_Vertex;
attribute lowp vec4 attr_Color;

// Uniforms
uniform highp mat4 u_modelViewProjectionMatrix;
uniform vec4 u_lightProjectionS;
uniform vec4 u_lightProjectionT;
uniform vec4 u_lightFalloff;
uniform vec4 u_lightProjectionQ;
uniform lowp vec4 u_colorModulate;
uniform lowp vec4 u_colorAdd;
uniform vec4 u_lightOrigin;
uniform vec4 u_viewOrigin;
uniform vec4 u_bumpMatrixS;
uniform vec4 u_bumpMatrixT;
uniform vec4 u_diffuseMatrixS;
uniform vec4 u_diffuseMatrixT;
uniform vec4 u_specularMatrixS;
uniform vec4 u_specularMatrixT;

// Out
// gl_Position
varying vec2 var_TexDiffuse;
varying vec2 var_TexNormal;
varying vec2 var_TexSpecular;
varying vec4 var_TexLight;
varying vec4 lowp var_Color;
varying vec3 var_L;
#if defined(BLINN_PHONG)
varying vec3 var_H;
#else
varying vec3 var_V;
#endif

void main(void)
{
	mat3 M = mat3(attr_Tangent, attr_Bitangent, attr_Normal);

	var_TexNormal.x = dot(u_bumpMatrixS, attr_TexCoord);
	var_TexNormal.y = dot(u_bumpMatrixT, attr_TexCoord);

	var_TexDiffuse.x = dot(u_diffuseMatrixS, attr_TexCoord);
	var_TexDiffuse.y = dot(u_diffuseMatrixT, attr_TexCoord);

	var_TexSpecular.x = dot(u_specularMatrixS, attr_TexCoord);
	var_TexSpecular.y = dot(u_specularMatrixT, attr_TexCoord);

	var_TexLight.x = dot(u_lightProjectionS, attr_Vertex);
	var_TexLight.y = dot(u_lightProjectionT, attr_Vertex);
	var_TexLight.z = dot(u_lightFalloff, attr_Vertex);
	var_TexLight.w = dot(u_lightProjectionQ, attr_Vertex);

	vec3 L = u_lightOrigin.xyz - attr_Vertex.xyz;
	vec3 V = u_viewOrigin.xyz - attr_Vertex.xyz;
#if defined(BLINN_PHONG)
	vec3 H = normalize(L) + normalize(V);
#endif

	var_L = L * M;
#if defined(BLINN_PHONG)
	var_H = H * M;
#else
    var_V = V * M;
#endif

	var_Color = (attr_Color / 255.0) * u_colorModulate + u_colorAdd;

	gl_Position = u_modelViewProjectionMatrix * attr_Vertex;
}
