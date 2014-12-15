#ifndef RAYTRACE_NEW_SL
#define RAYTRACE_NEW_SL

#ifndef GLSL
RaytracePixelOutput RaytraceNew(Texture3D cloudDensity1
											,Texture3D cloudDensity2
											,Texture2D rainMapTexture
											,Texture2D noiseTexture
											,Texture3D noiseTexture3D
											,Texture2D depthTexture
											,Texture2D lightTableTexture
											,Texture2D illuminationTexture
											,Texture2D rainbowLookupTexture
											,Texture2D coronaLookupTexture
											,Texture2D lossTexture
											,Texture2D inscTexture
											,Texture2D skylTexture
                                            ,bool do_depth_mix
											,vec2 texCoords
											,bool near_pass
											,bool noise
											,bool noise_3d
											,vec3 cloudIrRadiance1
											,vec3 cloudIrRadiance2)
{
	vec4 dlookup 		=texture_nearest_lod(depthTexture,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0);
	vec4 clip_pos		=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x			+=2.0*texCoords.x;
	clip_pos.y			-=2.0*texCoords.y;
	vec3 view			=normalize(mul(invViewProj,clip_pos).xyz);

	float s				=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDir		=lerp(directionToMoon,directionToSun,s);

	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	vec3 n				=vec3(clip_pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	vec2 noise_texc_0	=mul(noiseMatrix,vec4(n.xy,0,0)).xy/fractalRepeatLength;

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth;
	/*if(near_pass)
	{
		if(dlookup.z==0)
			discard;
		depth			=dlookup.y;
	}
	else
	{
		depth			=dlookup.x;
	}*/
	float d				=1.0;//depthToFadeDistance(depth,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec4 colour			=vec4(0.0,0.0,0.0,1.0);
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec2 nearFarTexc	=illum_lookup.xy;
	float meanFadeDistance	=0.0;
	// Precalculate hg effects
	float BetaClouds	=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh	=CalcRayleighBeta(cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);
#if USE_LIGHT_TABLES==0
	vec3 amb			=ambientColour.rgb;
#endif
	//project to the entry point:
	vec3 scales				=vec3(1.0,1.0,1.0)/inverseScales.xyz;
	vec3 zr					=vec3(0,0,0);
	vec3 dist_to_entry		=min(cornerPos+scales-viewPos,zr)+max(cornerPos-viewPos,zr);
	if(length(max(0,-dist_to_entry.xyz*view.xyz))>0)
	{
		discard;
	}
	dist_to_entry			/=view;
	// Now project to the nearest face of the cuboid:
	vec3 world_pos			=viewPos;
	dist_to_entry			=abs(dist_to_entry);
	float start_dist		=max(dist_to_entry.x,max(dist_to_entry.y,dist_to_entry.z));
	float dist				=start_dist;
	world_pos				+=view*start_dist;
	// Now test whether view is pointing away from the volume here:
	dist_to_entry			=min(cornerPos+scales-world_pos,zr)+max(cornerPos-world_pos,zr);
	if(length(max(0,-dist_to_entry.xyz*view.xyz))>0)
	{
		discard;
	}
	//vec3 testColour=step(dist_to_entry,vec3(start_dist,start_dist,start_dist));
	vec3 testColour=vec3(start_dist,start_dist,start_dist);
	for(int i=0;i<layerCount;i++)
	{
		vec4 density					=vec4(0,0,0,0);
		world_pos						=world_pos+200.0*view;
		dist							+=200.0;
		float fadeDistance				=saturate(dist/maxFadeDistanceMetres);
		vec3 cloudWorldOffset			=(world_pos-cornerPos);
		vec3 cloudTexCoords				=cloudWorldOffset*inverseScales;
		float layerFade					=1.0;
		if(cloudTexCoords.x<0||cloudTexCoords.x>1.0
			||cloudTexCoords.y<0||cloudTexCoords.y>1.0)
			layerFade=0;
		if(layerFade>0&&(fadeDistance<=d||!do_depth_mix)&&cloudTexCoords.z<=max_texc_z)
		{
			vec4 noiseval				=MakeNoise(noiseTexture3D,noise,1.0,cloudTexCoords);
			density						=calcDensity(cloudDensity1,cloudDensity2,cloudTexCoords,1.0,noiseval,fractalScale,cloud_interp);
            if(do_depth_mix)
				density.z				*=saturate((d-fadeDistance)/0.01);
			if(density.z>0)
			{
				float brightness_factor	;
				fade_texc.x				=sqrt(fadeDistance);
				vec4 c					=calcColour(lossTexture, inscTexture, skylTexture,lightTableTexture
													,density
													,BetaClouds,BetaRayleigh,BetaMie
													,lightResponse,ambientColour
													,world_pos,cloudTexCoords
													,fade_texc,nearFarTexc
													,brightness_factor);
				colour.rgb				+=c.rgb*c.a*(colour.a);
				meanFadeDistance		+=fadeDistance*c.a*colour.a;
				colour.a				*=(1.0-c.a);
				if(colour.a*brightness_factor<0.003)
				{
					colour.a			=0.0;
					break;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	meanFadeDistance					+=colour.a;
	RaytracePixelOutput res;
    res.colour							=vec4(exposure*colour.rgb,colour.a);
	res.depth							=fadeDistanceToDepth(meanFadeDistance,clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);
	return res;
}
#endif
#endif
