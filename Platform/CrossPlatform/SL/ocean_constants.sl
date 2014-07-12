#ifndef OCEAN_CONSTANTS_SL
#define OCEAN_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(cbImmutable,0)
	uniform uint g_ActualDim;
	uniform uint g_InWidth;
	uniform uint g_OutWidth;
	uniform uint g_OutHeight;
	uniform uint g_DxAddressOffset;
	uniform uint g_DyAddressOffset;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbChangePerFrame,1)
	uniform float g_Time;
	uniform float g_ChoppyScale;
	uniform float g_GridLen;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbShading,2)
	// The color of bottomless water body
	uniform vec3		g_WaterbodyColor;

	// The strength, direction and color of sun streak
	uniform float		g_Shineness;
	uniform vec3		g_SunDir;
	uniform vec3		g_SunColor;
	
	// The parameter is used for fixing an artifact
	uniform vec3		g_BendParam;

	// Perlin noise for distant wave crest
	uniform float		g_PerlinSize;
	uniform vec3		g_PerlinAmplitude;
	uniform vec3		g_PerlinOctave;
	uniform vec3		g_PerlinGradient;

	// Constants for calculating texcoord from position
	uniform float		g_TexelLength_x2;
	uniform float		g_UVScale;
	uniform float		g_UVOffset;
SIMUL_CONSTANT_BUFFER_END

// Per draw call constants
SIMUL_CONSTANT_BUFFER(cbChangePerCall,4)
	// Transform matrices
	uniform mat4	g_matLocal;
	uniform mat4	g_matWorldViewProj;
	uniform mat4	g_matWorld;

	// Misc per draw call constants
	uniform vec2		g_UVBase;
	uniform vec2		g_PerlinMovement;
	uniform vec3		g_LocalEye;

	// Atmospherics
	uniform float		hazeEccentricity;
	uniform vec3		lightDir;
	uniform vec4		mieRayleighRatio;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(OsdConstants,5)
	uniform vec4 rect;
	uniform float showMultiplier;
	uniform float agaher,reajst,aejtae;
SIMUL_CONSTANT_BUFFER_END

#endif