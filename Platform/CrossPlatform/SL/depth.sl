#ifndef DEPTH_SL
#define DEPTH_SL

// Enable the following to use a 3-parameter depth conversion, for possible slight speed improvement
#define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z

float depthToLinearDistance(float depth,vec3 depthToLinFadeDistParams)
{
#ifdef REVERSE_DEPTH
	if(depth<=0)
		return 1.0;//max_fade_distance_metres;
#else
	if(depth>=1.0)
		return 1.0;//max_fade_distance_metres;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	return linearFadeDistanceZ;
}

vec4 depthToLinearDistance(vec4 depth,vec3 depthToLinFadeDistParams)
{
	vec4 linearFadeDistanceZ = depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz);
	
	linearFadeDistanceZ=min(vec4(1.0,1.0,1.0,1.0),linearFadeDistanceZ);
	return linearFadeDistanceZ;
}

vec2 depthToLinearDistance(vec2 depth,vec3 depthToLinFadeDistParams)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz);
	linearFadeDistanceZ=min(vec2(1.0,1.0),linearFadeDistanceZ);
	return linearFadeDistanceZ;
}

void discardOnFar(float depth)
{
#ifdef REVERSE_DEPTH
	if(depth<=0)
		discard;
#else
	if(depth>=1.0)
		discard;
#endif
}

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef VISION
	float dist=nearZ+depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef REVERSE_DEPTH
	if(depth<=0)
		return 1.0;
#else
	if(depth>=1.0)
		return 1.0;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
#endif
}
/*float depthToFadeDistance(float depth,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(depth<=0)
		return 1.0;
#else
	if(depth>=1.0)
		return 1.0;
#endif
#ifdef VISION
	float dist=depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef REVERSE_DEPTH
	float linearFadeDistanceZ = nearZ*farZ/(nearZ+(farZ-nearZ)*depth);
#else
	float linearFadeDistanceZ = nearZ*farZ/(farZ-(farZ-nearZ)*depth);
#endif
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	
	return fadeDist;
#endif
}
*/
float fadeDistanceToDepth(float dist,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return 1.0;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z) / depthToLinFadeDistParams.y;
	return depth;
}


float fadeDistanceToDepth(float dist,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return 1.0;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
#ifdef REVERSE_DEPTH
	float depth				=((nearZ*farZ)/linearDistanceZ-nearZ)/(farZ-nearZ);
#else
	float depth				=((nearZ*farZ)/linearDistanceZ-farZ)/(nearZ-farZ);
#endif
	return depth;
}

vec2 viewportCoordToTexRegionCoord(vec2 iViewportCoord,vec4 iViewportToTexRegionScaleBias )
{
	return iViewportCoord * iViewportToTexRegionScaleBias.xy + iViewportToTexRegionScaleBias.zw;
}

#ifndef GLSL
#ifndef DX9

void GetCoordinates(Texture2D t,vec2 texc,out int2 pos2)
{
	uint2 dims;
	t.GetDimensions(dims.x,dims.y);
	pos2		=int2(texc*vec2(dims.xy));
}

void GetMSAACoordinates(Texture2DMS<vec4> textureMS,vec2 texc,out int2 pos2,out int numSamples)
{
	uint2 dims;
	uint nums;
	textureMS.GetDimensions(dims.x,dims.y,nums);
	numSamples=nums;
	pos2		=int2(texc*vec2(dims.xy));
}

void GetMSAACoordinates(Texture2DMS<vec4> textureMS,vec2 texc,out int2 pos2,out uint numSamples)
{
	uint2 dims;
	textureMS.GetDimensions(dims.x,dims.y,numSamples);
	pos2		=int2(texc*vec2(dims.xy));
}

// Filter the texture, but bias the result towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture,Texture2D depthTexture,vec2 lowResDims,vec2 texc,vec4 depthMask,vec3 depthToLinFadeDistParams,float d)
{
	vec2 texc_unit	=texc*lowResDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	vec2 xy			=frac(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,lowResDims.x-1);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,lowResDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);
	// x = right, y = up, z = left, w = down
	vec4 f11		=imageTexture[i11];
	vec4 f21		=imageTexture[i21];
	vec4 f12		=imageTexture[i12];
	vec4 f22		=imageTexture[i22];

	float d11		=depthToLinearDistance(dot(depthTexture[i11],depthMask),depthToLinFadeDistParams);
	float d21		=depthToLinearDistance(dot(depthTexture[i21],depthMask),depthToLinFadeDistParams);
	float d12		=depthToLinearDistance(dot(depthTexture[i12],depthMask),depthToLinFadeDistParams);
	float d22		=depthToLinearDistance(dot(depthTexture[i22],depthMask),depthToLinFadeDistParams);

	// But now we modify these values:
	float D1		=saturate((d-d11)/(d21-d11));
	float delta1	=abs(d21-d11);			
	f11				=lerp(f11,f21,delta1*D1);
	f21				=lerp(f21,f11,delta1*(1-D1));
	float D2		=saturate((d-d12)/(d22-d12));
	float delta2	=abs(d22-d12);			
	f12				=lerp(f12,f22,delta2*D2);
	f22				=lerp(f22,f12,delta2*(1-D2));

	vec4 f1			=lerp(f11,f21,xy.x);
	vec4 f2			=lerp(f12,f22,xy.x);
	
	float d1		=lerp(d11,d21,xy.x);
	float d2		=lerp(d12,d22,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);

	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);

	return f;
}
// Blending (not just clouds but any low-resolution alpha-blended volumetric image) into a hi-res target.
// Requires a near and a far image, a low-res depth texture with far (true) depth in the x, near depth in the y and edge in the z;
// a hi-res MSAA true depth texture.
vec4 NearFarDepthCloudBlend(vec2 texCoords
							,Texture2D lowResFarTexture
							,Texture2D lowResNearTexture
							,Texture2D hiResDepthTexture
							,Texture2D lowResDepthTexture
							,Texture2D<vec4> depthTexture
							,Texture2DMS<vec4> depthTextureMS
							,vec4 viewportToTexRegionScaleBias
							,vec3 depthToLinFadeDistParams
							,vec4 hiResToLowResTransformXYWH
							,Texture2D farInscatterTexture
							,Texture2D nearInscatterTexture
							,bool use_msaa)
{
	// texCoords.y is positive DOWNwards
	uint width,height;
	lowResNearTexture.GetDimensions(width,height);
	vec2 lowResDims	=vec2(width,height);

	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 hires_depth_pos2;
	int numSamples;
	if(use_msaa)
		GetMSAACoordinates(depthTextureMS,depth_texc,hires_depth_pos2,numSamples);
	else
	{
		GetCoordinates(depthTexture,depth_texc,hires_depth_pos2);
		numSamples=1;
	}
	vec2 lowResTexCoords		=hiResToLowResTransformXYWH.xy+hiResToLowResTransformXYWH.zw*texCoords;
	// First get the values that don't vary with MSAA sample:
	vec4 cloudFar;
	vec4 cloudNear				=vec4(0,0,0,1.0);
	vec4 lowres					=texture_clamp_lod(lowResDepthTexture,lowResTexCoords,0);
	vec4 hires					=texture_clamp_lod(hiResDepthTexture,texCoords,0);
	float lowres_edge			=lowres.z;
	float hires_edge			=hires.z;
	vec4 result					=vec4(0,0,0,0);
	vec2 nearFarDistLowRes		=depthToLinearDistance(lowres.yx,depthToLinFadeDistParams);
	vec4 insc					=vec4(0,0,0,0);
	vec4 insc_far				=texture_nearest_lod(farInscatterTexture,texCoords,0);
	vec4 insc_near				=texture_nearest_lod(nearInscatterTexture,texCoords,0);
	if(lowres_edge>0.0)
	{
		vec2 nearFarDistHiRes	=vec2(1.0,0.0);
		for(int i=0;i<numSamples;i++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth=depthTextureMS.Load(hires_depth_pos2,i).x;
			else
				hiresDepth=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		// Find the near and far depths at full resolution.
			if(trueDist<nearFarDistHiRes.x)
				nearFarDistHiRes.x=trueDist;
			if(trueDist>nearFarDistHiRes.y)
				nearFarDistHiRes.y=trueDist;
		}
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		float hiResInterp	=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth	=depthTextureMS.Load(hires_depth_pos2,j).x;
			else
				hiresDepth	=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			cloudNear		=depthDependentFilteredImage(lowResNearTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(0,1.0,0,0),depthToLinFadeDistParams,trueDist);
			cloudFar		=depthDependentFilteredImage(lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist);
			float interp	=saturate((nearFarDistLowRes.y-trueDist)/abs(nearFarDistLowRes.y-nearFarDistLowRes.x));
			vec4 add		=lerp(cloudFar,cloudNear,lowres_edge*interp);
			result			+=add;
		
			if(use_msaa)
			{
				hiResInterp		=hires_edge*saturate((nearFarDistHiRes.y-trueDist)/(nearFarDistHiRes.y-nearFarDistHiRes.x));
				insc			=lerp(insc_far,insc_near,hiResInterp);
				result.rgb		+=insc.rgb*add.a;
			}
		}
		// atmospherics: we simply interpolate.
		result				/=float(numSamples);
		hiResInterp			/=float(numSamples);
		if(!use_msaa)
		{
			result.rgb		+=insc_far.rgb*result.a;
		}
	}
	else
	{
		float hiresDepth=0.0;
		// Just use the zero MSAA sample if we're not at an edge:
		if(use_msaa)
			hiresDepth			=depthTextureMS.Load(hires_depth_pos2,0).x;
		else
			hiresDepth			=depthTexture[hires_depth_pos2].x;
		float trueDist		=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		result				=depthDependentFilteredImage(lowResFarTexture,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist);
		result.rgb			+=insc_far.rgb*result.a;
	}
    return result;
}

#endif
#endif
#endif