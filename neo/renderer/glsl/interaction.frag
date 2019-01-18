#version 100
precision mediump float;

// Option to use Half Lambert for shading
//#define HALF_LAMBERT

// Option to use Blinn Phong instead Gouraud
//#define BLINN_PHONG

// In
varying vec2 var_TexDiffuse;
varying vec2 var_TexNormal;
varying vec2 var_TexSpecular;
varying vec4 var_TexLight;
varying lowp vec4 var_Color;
varying vec3 var_L;
#if defined(BLINN_PHONG)
varying vec3 var_H;
#else
varying vec3 var_V;
#endif

// Uniforms
uniform lowp vec4 u_diffuseColor;
uniform lowp vec4 u_specularColor;
//uniform float u_specularExponent;
uniform sampler2D u_fragmentMap0;	/* u_bumpTexture */
uniform sampler2D u_fragmentMap1;	/* u_lightFalloffTexture */
uniform sampler2D u_fragmentMap2;	/* u_lightProjectionTexture */
uniform sampler2D u_fragmentMap3;	/* u_diffuseTexture */
uniform sampler2D u_fragmentMap4;	/* u_specularTexture */

// Out
// gl_FragCoord

void main(void)
{
	float u_specularExponent = 4.0;

	vec3 L = normalize(var_L);
#if defined(BLINN_PHONG)
	vec3 H = normalize(var_H);
	vec3 N = 2.0 * texture2D(u_fragmentMap0, var_TexNormal.st).agb - 1.0;
#else
	vec3 V = normalize(var_V);
	vec3 N = normalize(2.0 * texture2D(u_fragmentMap0, var_TexNormal.st).agb - 1.0);
#endif

	float NdotL = clamp(dot(N, L), 0.0, 1.0);
#if defined(HALF_LAMBERT)
	NdotL *= 0.5;
	NdotL += 0.5;
	NdotL = NdotL * NdotL;
#endif
#if defined(BLINN_PHONG)
	float NdotH = clamp(dot(N, H), 0.0, 1.0);
#endif

	vec3 lightProjection = texture2DProj(u_fragmentMap2, var_TexLight.xyw).rgb;
	vec3 lightFalloff = texture2D(u_fragmentMap1, vec2(var_TexLight.z, 0.5)).rgb;
	vec3 diffuseColor = texture2D(u_fragmentMap3, var_TexDiffuse).rgb * u_diffuseColor.rgb;
	vec3 specularColor = 2.0 * texture2D(u_fragmentMap4, var_TexSpecular).rgb * u_specularColor.rgb;

#if defined(BLINN_PHONG)
	float specularFalloff = pow(NdotH, u_specularExponent);
#else
	vec3 R = -reflect(L, N);
	float RdotV = clamp(dot(R, V), 0.0, 1.0);
	float specularFalloff = pow(RdotV, u_specularExponent);
#endif

	vec3 color;
	color = diffuseColor;
	color += specularFalloff * specularColor;
	color *= NdotL * lightProjection;
	color *= lightFalloff;

	gl_FragColor = vec4(color, 1.0) * var_Color;
}
