//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMPOSITE_SL
#define COMPOSITE_SL
#define DEBUG_COMPOSITING


#ifndef PI
#define PI (3.1415926536)
#endif

struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};

struct LookupQuad4
{
	vec4 _11;
	vec4 _21;
	vec4 _12;
	vec4 _22;
};

#define VOLUME_INSCATTER
#define SCREENSPACE_VOL
TwoColourCompositeOutput CompositeAtmospherics(vec4 clip_pos
				,vec2 depth_texc
				,TextureCube farCloudTexture
				,TextureCube nearCloudTexture
				,TextureCube nearFarTexture
				,TextureCube lightpassTexture
				,Texture2D loss2dTexture
				,Texture2D depthTexture
				,mat4 invViewProj
				,vec3 viewPos
				,mat4 invShadowMatrix
				,DepthIntepretationStruct depthInterpretationStruct
				,vec2 lowResTexCoords
				,Texture3D inscatterVolumeTexture
				,Texture2D shadowTexture
				,float maxFadeDistanceKm
				,float cloud_shadow)
{
	TwoColourCompositeOutput res;
	vec3 view					=normalize(mul(invViewProj,clip_pos).xyz);
	// we only care about view.z, i.e. the third element of the vector.
	// so only dot-product the third row of invViewProj, with clip_pos.
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[0][2],invViewProj[1][2],invViewProj[2][2],invViewProj[3][2]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_cube_lod(nearFarTexture	,view		,0);

	float depth					=texture_wrap_nearest_lod(depthTexture,depth_texc,0).x;

	float dist					=depthToFadeDistance(depth,clip_pos.xy,depthInterpretationStruct,tanHalfFov);
	float dist_rt				=pow(dist,0.5);
	vec4 cloud					=texture_cube_lod(farCloudTexture,view,0);

	vec3 lightspaceView			=normalize((mul(worldToScatteringVolumeMatrix,vec4(view,1.0))).xyz);
	float ls_elev				=asin(lightspaceView.z);
	vec3 volumeTexCoords		=vec3(atan2(lightspaceView.x,lightspaceView.y)/(2.0*pi),0.5*(1.0+2.0*ls_elev/pi),sqrt(dist*max(0.3,cos(ls_elev))));
	vec4 insc					=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);

	vec2 loss_texc				=vec2(dist_rt,0.5*(1.f-sine));
	float hiResInterp			=1.0-pow(saturate(( nearFarCloud.x- dist) / max(0.00001,nearFarCloud.x-nearFarCloud.y)),1.0);
	// we're going to do TWO interpolations. One from zero to the near distance,
	// and one from the near to the far distance.
	float nearInterp = pow(saturate((dist ) / 0.0033),1.0);
	//saturate((dist-nearFarCloud.x)/nearFarCloud.y);
		//
	
	vec4 cloudNear				=texture_cube_lod(nearCloudTexture, view, 0);
	//if(lowResTexCoords.x + lowResTexCoords.y<1.0)
		cloud						=lerp(cloudNear, cloud,hiResInterp);
		cloud = lerp(vec4(0, 0, 0, 1.0), cloud, nearInterp);
	
	vec4 lp						=texture_cube_lod(lightpassTexture,view,0);
	cloud.rgb					+=lp.rgb;
	//if(lowResTexCoords.x + lowResTexCoords.y>1.1)
	//	cloud = cloudNear;
//	if(lowResTexCoords.x+lowResTexCoords.y>1.0)
//		cloud=cloudNear;

	vec3 worldPos				=viewPos+view*dist*1000.0*maxFadeDistanceKm;
	float shadow				=lerp(1.0,GetSimpleIlluminationAt(shadowTexture,invShadowMatrix,worldPos).x,cloud_shadow);

	insc.rgb					*=cloud.a;
	insc						+=cloud;
	res.multiply				=texture_clamp_mirror_lod(loss2dTexture,loss_texc,0)*cloud.a*shadow;
	res.add = insc;
	/*if(lowResTexCoords.x + lowResTexCoords.y>1.1)
		res.add.rg= hiResInterp;*/
	//if (lowResTexCoords.x + lowResTexCoords.y>1.2)
	//		res.add.rgb = hiResInterp;
    return res;
}

TwoColourCompositeOutput CompositeAtmospherics_MSAA(vec4 clip_pos
													,vec2 depth_texc
													,TextureCube farCloudTexture
													,TextureCube nearCloudTexture
													,TextureCube nearFarTexture
													,Texture2D loss2dTexture
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,uint2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,DepthIntepretationStruct depthInterpretationStruct
													,vec2 lowResTexCoords
													,vec4 fullResToLowResTransformXYWH
													,Texture3D inscatterVolumeTexture
													,Texture2D shadowTexture)
{
	TwoColourCompositeOutput res;
	vec3 view					=mul(invViewProj,clip_pos).xyz;
	vec3 lightspaceView			=normalize((mul(worldToScatteringVolumeMatrix,vec4(view,1.0))).xyz);
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[0][2],invViewProj[1][2],invViewProj[2][2],invViewProj[3][2]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_cube_lod(nearFarTexture	,view		,0);
	
	float dd					=abs(nearFarCloud.x-nearFarCloud.y);
	vec4 cloud					=texture_cube_lod(farCloudTexture,view,0);
	vec4 cloudNear				=texture_cube_lod(nearCloudTexture, view, 0);
	
	float hires_edge			=dd;
	res.add						=vec4(0,0,0,1.0);
	res.multiply				=vec4(0,0,0,0);
	for(int k=0;k<numSamples;k++)
	{
		float depth				=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;

		float dist				=depthToLinearDistance(depth	,depthInterpretationStruct);
		float dist_rt			=pow(dist,0.5);
	
		vec3 volumeTexCoords	=vec3(lowResTexCoords, dist_rt);
		vec2 loss_texc			=vec2(dist_rt,0.5*(1.f-sine));
#if 1
		volumeTexCoords			=vec3(atan2(lightspaceView.x,lightspaceView.y)/(2.0*pi),0.5*(1.0+2.0*asin(lightspaceView.z)/pi),dist_rt);
		vec4 insc				=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#else
		vec4 insc				=texture_3d_wwc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#endif
		float hiResInterp		=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
		// we're going to do TWO interpolations. One from zero to the near distance,
		// and one from the near to the far distance.
		float nearInterp		=saturate(dist / max(nearFarCloud.y,0.000001));
	
		cloud					=lerp(cloudNear, cloud, hiResInterp);
	
		cloud					=lerp(vec4(0,0,0,1.0),cloud,nearInterp);
		float shadowInterp		=saturate((dist - nearFarCloud.w) / max(nearFarCloud.z-nearFarCloud.w,0.000001));
		//float hiResInterp		=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
#if 1
		vec4 shadow_lookup		=vec4(1.0, 1.0, 0, 0);
		shadow_lookup			=texture_wrap_lod(shadowTexture, lowResTexCoords, 0);
		float shadow			=shadow_lookup.x;

		shadow					=lerp(shadow_lookup.y,shadow_lookup.x,shadowInterp);
		
#else
		float shadow		=GetSimpleIlluminationAt(cloudShadowTexture,invShadowMatrix,worldPos).x;

#endif
		insc.rgb				*=cloud.a;
		insc					+=cloud;
		res.multiply			+=texture_wrap_lod(loss2dTexture,loss_texc,0)*cloud.a;//*shadow;
		res.add					+=insc;
	}
	res.multiply/=float(numSamples);
	res.add/=float(numSamples);
    return res;
}
#endif
