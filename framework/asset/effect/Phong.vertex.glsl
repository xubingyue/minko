#ifdef VERTEX_SHADER

#ifdef GL_ES
    #ifdef GL_FRAGMENT_PRECISION_HIGH
        precision highp float;
    #else
        precision mediump float;
    #endif
#endif

#pragma include "Skinning.function.glsl"
#pragma include "Pop.function.glsl"
#pragma include "Pack.function.glsl"

attribute 	vec3 	aPosition;
attribute 	vec2 	aUV;
attribute 	vec3 	aNormal;
attribute 	vec3 	aTangent;
attribute 	vec4	aBoneWeightsA;
attribute 	vec4	aBoneWeightsB;
attribute	float	aColor;

uniform 	mat4 	uModelToWorldMatrix;
uniform 	mat4 	uWorldToScreenMatrix;
uniform 	vec2 	uUVScale;
uniform 	vec2 	uUVOffset;
uniform 	mat4 	uLightWorldToScreenMatrix;

uniform 	float 	uPopLod;
uniform 	float 	uPopBlendingLod;
uniform 	float 	uPopFullPrecisionLod;
uniform 	vec3 	uPopMinBound;
uniform 	vec3 	uPopMaxBound;

varying 	vec3 	vertexPosition;
varying 	vec4 	vertexScreenPosition;
varying 	vec2 	vertexUV;
varying 	vec3 	vertexNormal;
varying 	vec3 	vertexTangent;
varying		vec4	vertexColor;

void main(void)
{
	#if defined DIFFUSE_MAP || defined NORMAL_MAP || defined SPECULAR_MAP || defined ALPHA_MAP
		vertexUV = uUVScale * aUV + uUVOffset;
	#endif // defined DIFFUSE_MAP || defined NORMAL_MAP || defined SPECULAR_MAP || defined ALPHA_MAP

	#if defined VERTEX_COLOR
		vertexColor = vec4(packFloat8bitRGB(aColor), 1.0);
	#endif // VERTEX_COLOR

	vec4 worldPosition = vec4(aPosition, 1.0);

	#ifdef SKINNING_NUM_BONES
		worldPosition = skinning_moveVertex(worldPosition, aBoneWeightsA, aBoneWeightsB);
	#endif // SKINNING_NUM_BONES

	#ifdef POP_LOD_ENABLED
		#ifdef POP_BLENDING_ENABLED
			worldPosition = pop_blend(worldPosition, aNormal, uPopLod, uPopBlendingLod, uPopFullPrecisionLod, uPopMinBound, uPopMaxBound);
		#else
			worldPosition = pop_quantify(worldPosition, aNormal, uPopLod, uPopFullPrecisionLod, uPopMinBound, uPopMaxBound);
		#endif // POP_BLENDING_ENABLED
	#endif // POP_LOD_ENABLED

	#ifdef MODEL_TO_WORLD
		worldPosition 	= uModelToWorldMatrix * worldPosition;
	#endif // MODEL_TO_WORLD

	#if defined NUM_DIRECTIONAL_LIGHTS || defined NUM_POINT_LIGHTS || defined NUM_SPOT_LIGHTS || defined ENVIRONMENT_MAP_2D || defined ENVIRONMENT_CUBE_MAP

		vertexPosition = worldPosition.xyz;
		vertexNormal = aNormal;

		#ifdef SKINNING_NUM_BONES
			vertexNormal = skinning_moveVertex(vec4(aNormal, 0.0), aBoneWeightsA, aBoneWeightsB).xyz;
		#endif // SKINNING_NUM_BONES

		#ifdef MODEL_TO_WORLD
			vertexNormal = mat3(uModelToWorldMatrix) * vertexNormal;
		#endif // MODEL_TO_WORLD
		vertexNormal = normalize(vertexNormal);

		#ifdef NORMAL_MAP
			vertexTangent = aTangent;
			#ifdef MODEL_TO_WORLD
				vertexTangent = mat3(uModelToWorldMatrix) * vertexTangent;
			#endif // MODEL_TO_WORLD
			vertexTangent = normalize(vertexTangent);
		#endif // NORMAL_MAP

	#endif // NUM_DIRECTIONAL_LIGHTS || NUM_POINT_LIGHTS || NUM_SPOT_LIGHTS || ENVIRONMENT_MAP_2D || ENVIRONMENT_CUBE_MAP

	vec4 screenPosition = uWorldToScreenMatrix * worldPosition;

	vertexScreenPosition = screenPosition;

	gl_Position = screenPosition;
}

#endif // VERTEX_SHADER
