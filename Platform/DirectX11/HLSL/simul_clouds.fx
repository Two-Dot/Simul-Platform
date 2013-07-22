#include "CppHLSL.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/simul_clouds.sl"
#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/earth_shadow_fade.sl"
#define Z_VERTICAL 1
#ifndef WRAP_CLOUDS
	#define WRAP_CLOUDS 1
#endif
#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif

SamplerState lightningSamplerState
{
    Texture = <lightningIlluminationTexture>;
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Border;
	AddressV = Border;
	AddressW = Border;
};

struct RaytraceVertexOutput
{
    float4 hPosition		: SV_POSITION;
	float2 texCoords		: TEXCOORD0;
};

RaytraceVertexOutput VS_Raytrace(idOnly IN)
{
    RaytraceVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}

vec4 calcDensity1(vec3 texCoords,float layerFade,vec3 noiseval)
{
	vec3 pos=texCoords.xyz+fractalScale.xyz*noiseval;
	vec4 density=sampleLod(cloudDensity,cloudSamplerState,pos,0);
	density.z*=layerFade;
	//density.z=saturate(density.z*(1.f+alphaSharpness)-alphaSharpness);
	return density;
}

float4 PS_Raytrace(RaytraceVertexOutput IN) : SV_TARGET
{
	float2 texCoords	=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	float4 dlookup		=sampleLod(depthTexture,clampSamplerState,texCoords.xy,0);
	float4 pos			=float4(-1.f,-1.f,1.f,1.f);
	pos.x				+=2.f*IN.texCoords.x;
	pos.y				+=2.f*IN.texCoords.y;
	float3 view			=normalize(mul(invViewProj,pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	float3 n			=float3(pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	float2 noise_texc_0	=mul((float2x2)noiseMatrix,n.xy);

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth			=dlookup.r;
	float d				=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	float4 colour		=float4(0.0,0.0,0.0,1.0);
	float Z				=0.f;
	float2 fade_texc	=float2(0.0,0.5*(1.0-sine));

	// Lookup in the illumination texture.
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illuminationTexture,illum_texc).rg;
	// This provides the range of texcoords that is lit.
	for(int i=0;i<layerCount;i++)
	//int i=40;
	{
		const LayerData layer=layers[i];
		float dist=layer.layerDistance;
		float z=saturate(dist/maxFadeDistanceMetres);
		if(z<d)
		//if(i==32)
		{
			float3 pos=viewPos+dist*view;
			pos.z-=layer.verticalShift;
			float4 texCoords;
			texCoords.xyz=(pos-cornerPos)*inverseScales;
			texCoords.w=texCoords.z;
			if(texCoords.z>=min_texc_z&&texCoords.z<=max_texc_z)
			{
				float2 noise_texc	=noise_texc_0*layer.noiseScale+layer.noiseOffset;
				float3 noiseval		=texCoords.w*noiseTexture.SampleLevel(noiseSamplerState,noise_texc.xy,0).xyz;
				float4 density		=calcDensity(texCoords.xyz,layer.layerFade,noiseval);
				if(density.z>0)
				{
					float4 c=calcColour(density,cos0,texCoords.z);
					fade_texc.x=sqrt(z);
					float sh=saturate((fade_texc.x-nearFarTexc.x)/0.1);
					c.rgb*=sh;
					c.rgb=applyFades(c.rgb,fade_texc,cos0,sh);
					colour*=(1.0-c.a);
					colour.rgb+=c.rgb*c.a;
					Z*=(1.0-c.a);
					Z+=z*c.a;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	fade_texc.x=sqrt(Z);
	//colour.rgb=(1.0-colour.a)*applyFades(colour.rgb,fade_texc,cos0,earthshadowMultiplier);
    return float4(exposure*colour.rgb,1.0-colour.a);
}
float4 PS_SimpleRaytrace(RaytraceVertexOutput IN) : SV_TARGET
{
	float2 texCoords	=IN.texCoords.xy;
	texCoords.y			=1.0-texCoords.y;
	float4 dlookup		=sampleLod(depthTexture,clampSamplerState,texCoords.xy,0);
	float4 pos			=float4(-1.f,-1.f,1.f,1.f);
	pos.x				+=2.f*IN.texCoords.x;
	pos.y				+=2.f*IN.texCoords.y;
	float3 view			=normalize(mul(invViewProj,pos).xyz);
	float cos0			=dot(lightDir.xyz,view.xyz);
	float sine			=view.z;
	float3 n			=float3(pos.xy*tanHalfFov,1.0);
	n					=normalize(n);
	float2 noise_texc_0	=mul((float2x2)noiseMatrix,n.xy);

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth			=dlookup.r;
	float d				=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	float4 colour		=float4(0.0,0.0,0.0,1.0);
	float Z				=0.f;
	float2 fade_texc	=float2(0.0,0.5*(1.0-sine));
	for(int i=0;i<layerCount;i++)
	{
		const LayerData layer=layers[i];
		float dist=layer.layerDistance;
		float z=saturate(dist/maxFadeDistanceMetres);
		if(z<d)
		{
			float3 pos=viewPos+dist*view;
			pos.z-=layer.verticalShift;
			float3 texCoords=(pos-cornerPos)*inverseScales;
			if(texCoords.z>=min_texc_z&&texCoords.z<=max_texc_z)
			{
				float4 density		=calcDensity(texCoords,layer.layerFade,vec3(0,0,0));
				if(density.z>0)
				{
					float4 c=calcColour(density,cos0,texCoords.z);
					fade_texc.x=sqrt(z);
					colour*=(1.0-c.a);
					colour.rgb+=c.rgb*c.a;
					Z*=(1.0-c.a);
					Z+=z*c.a;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
	fade_texc.x=sqrt(Z);
	colour.rgb=exposure*(1.0-colour.a)*applyFades(colour.rgb,fade_texc,cos0,earthshadowMultiplier);
    return float4(colour.rgb,1.0-colour.a);
}

float4 PS_Raytrace3DNoise(RaytraceVertexOutput IN) : SV_TARGET
{
	float2 texCoords=IN.texCoords.xy;
	texCoords.y=1.0-texCoords.y;
	float4 dlookup=sampleLod(depthTexture,clampSamplerState,texCoords.xy,0);
	float4 pos=float4(-1.f,-1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y+=2.f*IN.texCoords.y;
	float3 view=normalize(mul(invViewProj,pos).xyz);
	float cos0=dot(lightDir.xyz,view.xyz);
	float sine=view.z;
	float3 n			=float3(pos.xy*tanHalfFov,1.0);
	n					=normalize(n);

	float min_texc_z	=-fractalScale.z*1.5;
	float max_texc_z	=1.0-min_texc_z;

	float depth=dlookup.r;
	float d=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	float4 colour=float4(0.0,0.0,0.0,1.0);
	
	for(int i=0;i<layerCount;i++)
	//int i=40;
	{
		const LayerData layer=layers[i];
		float dist=layer.layerDistance;
		float z=dist/300000.0;
		if(z<d)
		//if(i==12)
		{
			float3 pos=viewPos+dist*view;
			pos.z-=layer.verticalShift;
			float3 texCoords=(pos-cornerPos)*inverseScales;
			if(texCoords.z>=min_texc_z&&texCoords.z<=max_texc_z)
			{
				float3 noise_texc	=texCoords.xyz*noise3DTexcoordScale;
				float3 noiseval		=float3(0.0,0.0,0.0);
				float mul=0.5;
				for(int j=0;j<noise3DOctaves;j++)
				{
					noiseval		+=(noiseTexture3D.SampleLevel(noiseSamplerState,noise_texc,0).xyz)*mul;
					noise_texc*=2.0;
					mul*=noise3DPersistence;
				}
				float4 density		=calcDensity(texCoords,1.0,noiseval);
				if(density.z>0)
				{
					float4 c=calcColour(density,cos0,texCoords.z);
					float2 fade_texc=float2(sqrt(z),0.5*(1.0-sine));
					c.rgb=applyFades(c.rgb,fade_texc,cos0,earthshadowMultiplier);
					colour*=(1.0-c.a);
					colour.rgb+=c.rgb*c.a;
					//colour.rgb+=frac(noise_texc.xyy)*c.a;
				}
			}
		}
	}
	if(colour.a>=1.0)
	   discard;
    return float4(colour.rgb,1.0-colour.a);
}

struct vertexInput
{
    float3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset		: TEXCOORD0;
	vec2 elevationRange		: TEXCOORD1;
	float noiseScale		: TEXCOORD2;
	float layerFade			: TEXCOORD3;
	float layerDistance		: TEXCOORD4;
};

struct VStoGS
{
    float3 position			: POSITION;
	// Per-instance data:
	vec2 noiseOffset		: TEXCOORD0;
	float noiseScale		: TEXCOORD1;
	float layerFade			: TEXCOORD2;
	float layerDistance		: TEXCOORD3;
};

struct toPS
{
    float4 hPosition		: SV_POSITION;
    float2 noise_texc		: TEXCOORD0;
    float4 texCoords		: TEXCOORD1;
	float3 view				: TEXCOORD2;
    float3 texCoordLightning: TEXCOORD3;
    float2 fade_texc		: TEXCOORD4;
	float layerFade			: TEXCOORD5;
};

VStoGS VS_FeedToGS(vertexInput IN)
{
    VStoGS OUT;
	OUT.position=IN.position;
	// Per-instance data:
	OUT.noiseOffset		=IN.noiseOffset;
	OUT.noiseScale		=IN.noiseScale;
	OUT.layerFade		=IN.layerFade;
	OUT.layerDistance	=IN.layerDistance;
    return OUT;
}

toPS VS_Main(vertexInput IN)
{
    toPS OUT;
	float3 pos				=IN.position;
	if(pos.z<IN.elevationRange.x||pos.z>IN.elevationRange.y)
		pos*=0.f;
	OUT.view				=normalize(pos.xyz);
	float sine				=OUT.view.z;
	float3 t1				=pos.xyz*IN.layerDistance;
	OUT.hPosition			=float4(t1.xyz,1.0);
	float2 noise_pos		=mul((float2x2)noiseMatrix,pos.xy);
	OUT.noise_texc			=float2(atan2(noise_pos.x,1.0),atan2(noise_pos.y,1.0));
	OUT.noise_texc			*=IN.noiseScale;
	OUT.noise_texc			+=IN.noiseOffset;
	
	float3 wPos				=t1+viewPos;
	OUT.texCoords.xyz		=wPos-cornerPos;
	OUT.texCoords.xyz		*=inverseScales;
	OUT.texCoords.w			=saturate(OUT.texCoords.z);
	float3 texCoordLightning=(pos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning	=texCoordLightning;
	
	float depth				=IN.layerDistance/maxFadeDistanceMetres;
		
	OUT.fade_texc			=float2(sqrt(depth),0.5f*(1.f-sine));
	OUT.layerFade			=IN.layerFade;
    return OUT;
}
#define ELEV_STEPS 20

[maxvertexcount(53)]
void GS_Main(line VStoGS input[2], inout TriangleStream<toPS> OutputStream)
{
	/*if(input[0].texCoords.z>1.0&&input[1].texCoords.z>1.0&&input[2].texCoords.z>1.0)
		return;
	if(input[0].texCoords.z<0.0&&input[1].texCoords.z<0.0&&input[2].texCoords.z<0.0)
		return;*/
	// work out the start and end angles.
	float dh1=cornerPos.z-viewPos.z;
	float dh2=dh1+1.0/inverseScales.z;
	float a1=atan(dh1/input[0].layerDistance)*2.0/pi;
	float a2=atan(dh2/input[0].layerDistance)*2.0/pi;
	int e1=max((int)(a1*ELEV_STEPS/2+ELEV_STEPS/2)-1,0);
	int e2=min((int)(a2*ELEV_STEPS/2+ELEV_STEPS/2)+3,ELEV_STEPS);

	for(int i=e1;i<e2+1;i++)
	{
		float angle=pi*(float)(i-ELEV_STEPS/2)/(float)ELEV_STEPS;
		float cosine=cos(angle);
		float sine=sin(angle);
		float3 pos[2];
		float3 t1[2];
		float4 hpos[2];
		for(int j = 0; j < 2; j++)
		{
			VStoGS IN=input[j];
			float3 pos=cosine*IN.position;
			pos.z=sine;
			float3 t1=pos.xyz*IN.layerDistance;
			float4 hpos=(float4(t1.xyz,1.0));
			//float4 hpos=mul(worldViewProj,float4(t1.xyz,1.0));

			toPS OUT;
			OUT.hPosition			=hpos;
			float3 noise_pos		=float3(mul((float2x2)noiseMatrix,pos.xy).xy,1.0);
			OUT.noise_texc			=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));
			OUT.noise_texc			*=IN.noiseScale;
			OUT.noise_texc			+=IN.noiseOffset;
			
			OUT.view				=pos.xyz;
			float3 wPos				=viewPos+t1;
			OUT.texCoords.xyz		=wPos-cornerPos;
			OUT.texCoords.xyz		*=inverseScales;
			OUT.texCoords.w			=saturate(OUT.texCoords.z);
			float3 texCoordLightning=(pos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
			OUT.texCoordLightning	=texCoordLightning;
			float3 view				=normalize(pos.xyz);
			
			float depth				=IN.layerDistance/maxFadeDistanceMetres;
			OUT.fade_texc			=float2(sqrt(depth),0.5f*(1.f-sine));
			OUT.layerFade			=IN.layerFade;
    		OutputStream.Append(OUT);
		}
	}
}
float4 PS_Clouds( toPS IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

float4 PS_Clouds3DNoise( toPS IN): SV_TARGET
{
	float3 noise_texc	=IN.texCoords.xyz*noise3DTexcoordScale;
	float3 noiseval		=float3(0.0,0.0,0.0);
	float mul=0.5;
	for(int i=0;i<noise3DOctaves;i++)
	{
		noiseval		+=(noiseTexture3D.Sample(noiseSamplerState,noise_texc).xyz)*mul;
		noise_texc*=2.0;
		mul*=noise3DPersistence;
	}
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
    return final;
}

float4 PS_WithLightning(toPS IN): SV_TARGET
{
	float3 noiseval=(noiseTexture.Sample(noiseSamplerState,IN.noise_texc.xy).xyz).xyz;
#ifdef DETAIL_NOISE
	noiseval+=(noiseTexture.Sample(noiseSamplerState,8.0*IN.noise_texc.xy).xyz)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 view=normalize(IN.view);
	float cos0=dot(lightDir.xyz,view.xyz);
	float4 final=calcUnfadedColour(IN.texCoords.xyz,IN.layerFade,noiseval,cos0);

	float4 lightning=lightningIlluminationTexture.Sample(lightningSamplerState,IN.texCoordLightning.xyz);

	float l=dot(lightningMultipliers,lightning);
	float3 lightningC=l*lightningColour.xyz;
	final.rgb+=lightningColour.w*lightningC;
	
	final.rgb=applyFades(final.rgb,IN.fade_texc,cos0,earthshadowMultiplier);
	final.rgb*=final.a;

	final.rgb+=lightningC*(final.a+IN.layerFade);
    return final;
}

struct vertexInputCS
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    float4 hPosition		: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
};

//uniform_buffer SingleLayerConstants R12
//{
	uniform vec4 rect;
//};

vertexOutputCS VS_FullScreen(idOnly IN)
{
	vertexOutputCS OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,0.0,1.0);
	// Set to far plane
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(pos.x,-pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

// Given texture position from texCoords, convert to a worldpos with shadowMatrix.
// Then, trace towards sun to find initial intersection with cloud volume
// Then trace down to find first intersection with clouds, if any.
float4 PS_CloudShadow( vertexOutputCS IN):SV_TARGET
{
    vec3 pos			=80000.0*vec3(IN.texCoords.xy-vec2(0.5,0.5),0.0);
	vec3 wpos0			=mul(shadowMatrix,vec4(pos,1.0)).xyz;
	float dh			=cornerPos.z-wpos0.z+1.0/inverseScales.z;
	float thickness		=dh/lightDir.z;
	float illumination	=1.0;
	static const int C=256;
	for(int i=0;i<C;i++)
	{
		float u			=float(i)/float(C);
		float z			=80000.0*(u-0.5);
		vec3 wpos		=wpos0+lightDir.xyz*z;
		vec3 texc		=(wpos-cornerPos)*inverseScales;
		vec4 density1	=sampleLod(cloudDensity1,wwcSamplerState,texc,0);
		vec4 density2	=sampleLod(cloudDensity2,wwcSamplerState,texc,0);
		vec4 density	=lerp(density1,density2,cloud_interp);
		illumination	=lerp(illumination,0,density.z);
	}
	return float4(illumination,0,0,1.0);
}

vertexOutputCS VS_CrossSection(idOnly IN)
{
    vertexOutputCS OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(rect.xy+rect.zw*pos,0.0,1.0);
	//OUT.hPosition	=float4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=pos;
    return OUT;
}

float4 PS_Simple( vertexOutputCS IN):SV_TARGET
{
    return noiseTexture.Sample(samplerStateWrap,IN.texCoords.xy);
}

float4 PS_ShowNoise( vertexOutputCS IN):SV_TARGET
{
    float4 lookup=noiseTexture.Sample(samplerStateWrap,IN.texCoords.xy);
	return float4(0.5*(lookup.rgb+1.0),1.0);
}

#define CROSS_SECTION_STEPS 32
float4 PS_CrossSectionXZ( vertexOutputCS IN):SV_TARGET
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,0,crossSectionOffset.z+IN.texCoords.y);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.y+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=cloudDensity1.Sample(wwcSamplerState,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.y+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
}

float4 PS_CrossSectionXY( vertexOutputCS IN): SV_TARGET
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,crossSectionOffset.y+IN.texCoords.y,0);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.z+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=cloudDensity1.Sample(wwcSamplerState,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.x);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.z;//+.05f;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.z+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
}

BlendState CloudBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

technique11 simul_clouds
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds()));
    }
}

technique11 simul_raytrace
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_Raytrace()));
    }
}

technique11 simul_simple_raytrace
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_SimpleRaytrace()));
    }
}

technique11 simul_raytrace_3d_noise
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetVertexShader(CompileShader(vs_5_0,VS_Raytrace()));
		SetPixelShader(CompileShader(ps_5_0,PS_Raytrace3DNoise()));
    }
}

technique11 simul_clouds_gs
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds()));
    }
}

technique11 simul_clouds_3d_noise
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_Clouds3DNoise()));
    }
}

technique11 simul_clouds_and_lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		//SetBlendState(CloudBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(CompileShader(gs_5_0,GS_Main()));
		SetVertexShader(CompileShader(vs_5_0,VS_FeedToGS()));
		SetPixelShader(CompileShader(ps_5_0,PS_WithLightning()));
    }
}

technique11 cloud_shadow
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CloudShadow()));
    }
}
technique11 cross_section_xz
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CrossSectionXZ()));
    }
}

technique11 cross_section_xy
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState( RenderNoCull );
		SetBlendState(NoBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_CrossSectionXY()));
    }
}

technique11 simple
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Simple()));
    }
}

technique11 show_noise
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_CrossSection()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ShowNoise()));
    }
}