#ifndef SKY_CONSTANTS_SL
#define SKY_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SkyConstants,10)
	uniform mat4 worldViewProj;
	uniform mat4 proj;
	uniform mat4 cubemapViews[6];
	uniform mat4 invViewProj;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec4 eyePosition;
	uniform vec4 lightDir;
	uniform vec4 mieRayleighRatio;
	uniform vec4 colour;

	uniform float hazeEccentricity;
	uniform float skyInterp;
	uniform float altitudeTexCoord;
	uniform float starBrightness;

	uniform float radiusRadians;
	uniform float overcast;
	uniform float overcastBaseKm;
	uniform float overcastRangeKm;
	
	uniform uniform uint3 threadOffset;
	uniform float maxFadeDistanceKm;

	uniform float cloudShadowRange;		// As a distance texcoord
	uniform float illumination_alt_texc;	// Due to earth shadow
	uniform uint cycled_index;
	uniform float overlayAlpha;
SIMUL_CONSTANT_BUFFER_END

#endif