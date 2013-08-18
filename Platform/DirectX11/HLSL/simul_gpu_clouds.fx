#include "CppHLSL.hlsl"
uniform sampler2D inputTexture;
uniform sampler3D densityTexture SIMUL_TEXTURE_REGISTER(0);
uniform sampler3D lightTexture;
uniform sampler3D lightTexture1 SIMUL_TEXTURE_REGISTER(1);
uniform sampler3D lightTexture2 SIMUL_TEXTURE_REGISTER(2);
uniform sampler2D maskTexture SIMUL_TEXTURE_REGISTER(3);
uniform sampler3D ambientTexture;
uniform sampler3D ambientTexture1 SIMUL_TEXTURE_REGISTER(4);
uniform sampler3D ambientTexture2 SIMUL_TEXTURE_REGISTER(5);
uniform sampler3D volumeNoiseTexture SIMUL_TEXTURE_REGISTER(6);
RWTexture3D<float4> targetTexture SIMUL_RWTEXTURE_REGISTER(0);
RWTexture3D<float> targetTexture1 SIMUL_RWTEXTURE_REGISTER(1);

#include "../../CrossPlatform/states.sl"
#include "../../CrossPlatform/simul_gpu_clouds.sl"

SamplerState lightSamplerState : register(s8);

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texCoords	: TEXCOORD0;		
};

vertexOutput VS_Main(idOnly IN)
{
    vertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	pos.y			=yRange.x+pos.y*yRange.y;
	float4 vert_pos	=float4(float2(-1.0,1.0)+2.0*vec2(pos.x,-pos.y),1.0,1.0);
    OUT.hPosition	=vert_pos;
    OUT.texCoords	=pos;
    return OUT;
}

float4 PS_DensityMask(vertexOutput IN) : SV_TARGET
{
	float dr					=0.1;
	vec2 pos					=2.0*IN.texCoords.xy-vec2(1.0,1.0);
	float r						=length(pos);
	float dens					=saturate((1.0-r)/dr);
    return float4(dens,dens,dens,1.0);
}

float4 PS_Density(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texCoords.xy);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_val				=NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z)*maskTexture.Sample(clampSamplerState,densityspace_texcoord.xy).x;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    return vec4(dens,0,0,1.0);
}

[numthreads(8,8,8)]
void CS_Density(uint3 sub_pos				: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	uint3 pos			=sub_pos+threadOffset;
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	vec3 densityspace_texcoord	=(pos+0.5)/vec3(dims);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_val				=NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z);
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
	dens						=saturate(dens);
	targetTexture[pos]			=dens;
}

static const float glow=0.1;
float4 PS_Lighting(vertexOutput IN) : SV_TARGET
{
	vec2 texcoord				=IN.texCoords.xy;//+texCoordOffset;
	vec2 previous_light			=inputTexture.Sample(lightSamplerState,texcoord.xy).xy;
	vec3 lightspace_texcoord	=vec3(texcoord.xy,zPosition);
	vec3 densityspace_texcoord	=mul(transformMatrix,vec4(lightspace_texcoord,1.0)).xyz;
	float density				=texture_wwc(densityTexture,densityspace_texcoord).x;
	vec2 unity					=vec2(1.0,1.0);
	//if(density==0)
	//	previous_light			=unity-exp(-.1*zPixel)*(unity-previous_light);
	float direct_light			=previous_light.x*exp(-extinctions.x*density);
	float indirect_light		=previous_light.y*exp(-extinctions.y*density);
	//indirect_light				+=(direct_light+indirect_light)*glow*density;
    return						vec4(direct_light,indirect_light,0,0);
}

float4 PS_Transform(vertexOutput IN) : SV_TARGET
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(IN.texCoords.xy);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=mul(transformMatrix,vec4(densityspace_texcoord,1.0)).xyz;
	lightspace_texcoord.z		-=zPixel;
	vec2 light_lookup			=saturate(lightTexture.SampleLevel(lightSamplerState,lightspace_texcoord,0).xy);
	vec2 amb_texel				=ambientTexture.SampleLevel(wwcSamplerState,ambient_texcoord,0).xy;
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x);
	return						vec4(light_lookup.y,light_lookup.x,density,ambient_lookup);
}

[numthreads(8,8,1)]
void CS_Lighting(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float direct_light				=1.0;
	targetTexture1[int3(pos.xy,0)]				=1.0;
	for(int i=1;i<dims.z;i++)
	{
		uint3 idx					=uint3(pos.xy,i);
		vec3 lightspace_texcoord	=(vec3(idx)+0.5)/vec3(dims);
		vec3 texc					=(vec3(pos.xy,(float)i)+0.5)/vec3(dims);
		vec3 densityspace_texcoord	=(mul(transformMatrix,vec4(lightspace_texcoord,1.0))).xyz;
		float density				=densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x;
		//if(density==0)
		//	direct_light=1.0;
		targetTexture1[idx]			=direct_light;
		direct_light				*=exp(-extinctions.x*density*stepLength);
	}
}

[numthreads(8,8,1)]
void CS_SecondaryLighting(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float indirect_light			=1.0;
	if(pos.z>0)
	{
		int Z			=pos.z-1;
		int x1			=(pos.x+1)%dims.x;
		int xn			=(pos.x+dims.x-1)%dims.x;
		int y1			=(pos.y+1)%dims.y;
		int yn			=(pos.y+dims.y-1)%dims.y;
		indirect_light	=targetTexture1[int3(pos.xy,Z)];
		indirect_light	+=targetTexture1[int3(xn,pos.y,Z)];
		indirect_light	+=targetTexture1[int3(x1,pos.y,Z)];
		indirect_light	+=targetTexture1[int3(pos.x,yn,Z)];
		indirect_light	+=targetTexture1[int3(pos.x,y1,Z)];
		indirect_light	/=5.0;
	}
	//for(int i=0;i<dims.z;i++)
	int i=pos.z;
	{
		uint3 idx					=uint3(pos.xy,i);
		vec3 lightspace_texcoord	=(vec3(idx)+0.5)/vec3(dims);
		vec3 texc					=(vec3(pos.xy,(float)i)+0.5)/vec3(dims);
		vec3 densityspace_texcoord	=(mul(transformMatrix,vec4(lightspace_texcoord,1.0))).xyz;
		float density				=densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x;
		indirect_light				*=exp(-extinctions.y*density*stepLength);
		targetTexture1[idx]			=indirect_light;
		//if(density==0)
		//	indirect_light=1.0;
	}
}

[numthreads(8,8,8)]
void CS_Transform(uint3 sub_pos	: SV_DispatchThreadID)	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	uint3 pos=sub_pos+threadOffset;
	targetTexture[pos]	=vec4(1.0,1.0,1.0,1.0);
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	targetTexture[pos]			=vec4(1.0,1.0,1.0,1.0);
	vec3 densityspace_texcoord	=(pos.xyz+0.5)/vec3(dims);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=mul(transformMatrix,vec4(densityspace_texcoord,1.0)).xyz;
	lightspace_texcoord.z		-=zPixelLightspace;
	vec2 light_lookup			=vec2(lightTexture1.SampleLevel(lightSamplerState,lightspace_texcoord,0).x
										,lightTexture2.SampleLevel(lightSamplerState,lightspace_texcoord,0).x);
	vec2 amb_texel				=vec2(ambientTexture1.SampleLevel(wwcSamplerState,ambient_texcoord,0).x
										,ambientTexture2.SampleLevel(wwcSamplerState,ambient_texcoord,0).x);
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x);

    vec4 res					=vec4(light_lookup.y,light_lookup.x,density,ambient_lookup);
   // res							=vec4(lightspace_texcoord.zz,density,lightspace_texcoord.z);
	targetTexture[pos]			=res;
}


//------------------------------------
// Technique
//------------------------------------
DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};
BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};
RasterizerState RenderNoCull { CullMode = none; };

technique11 density_mask
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_DensityMask()));
    }
}
technique11 simul_gpu_density
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Density()));
    }
}

technique11 gpu_density_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Density()));
    }
}

technique11 simul_gpu_lighting
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Lighting()));
    }
}

technique11 simul_gpu_transform
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Transform()));
    }
}

technique11 gpu_lighting_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Lighting()));
    }
}

technique11 gpu_secondary_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_SecondaryLighting()));
    }
}

technique11 gpu_transform_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Transform()));
    }
}