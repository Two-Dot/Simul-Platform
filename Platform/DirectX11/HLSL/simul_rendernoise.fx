#include "CppHlsl.hlsl"
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/noise.sl"
#include "../../CrossPlatform/SL/noise_constants.sl"

Texture2D noise_texture SIMUL_TEXTURE_REGISTER(0);
Texture3D random_texture_3d SIMUL_TEXTURE_REGISTER(1);
RWTexture3D<vec4> targetTexture32 SIMUL_RWTEXTURE_REGISTER(0);
RWTexture3D<vec4> targetTexture8 SIMUL_RWTEXTURE_REGISTER(1);

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct a2v
{
    vec4 position  : POSITION;
    vec2 texCoords  : TEXCOORD0;
};

struct v2f
{
    vec4 hPosition  : SV_POSITION;
    vec2 texCoords  : TEXCOORD0;
};

v2f MainVS(idOnly IN)
{
    v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,pos.y));
	return OUT;
}

vec4 RandomPS(v2f IN) : SV_TARGET
{
	// Range from -1 to 1.
    vec4 c=2.0*vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords))-1.0;
    return c;
}

vec4 NoisePS(v2f IN) : SV_TARGET
{
    return Noise(noise_texture,IN.texCoords,persistence,octaves);
}

[numthreads(8,8,8)]
void CS_Random3D(uint3 pos	: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	targetTexture32.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec3 texCoords	=(vec3(pos)+vec3(0.5,0.5,0.5))/vec3(dims);
	vec2 texc2		=texCoords.xy+dims.y*texCoords.z;
	// Range from -1 to 1.
    vec4 c					=2.0*vec4(rand(texc2),rand(1.7*texc2),rand(0.11*texc2),rand(513.1*texc2))-vec4(1.0,1.0,1.0,1.0);
    targetTexture32[pos]	=c;
}

[numthreads(8,8,8)]
void CS_Noise3D(uint3 pos	: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	targetTexture8.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec4 result		=vec4(0,0,0,0);
	vec3 texCoords	=(vec3(pos)+vec3(0.5,0.5,0.5))/vec3(dims);
	float mult		=0.5;
	float total		=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c		=texture_wrap_lod(random_texture_3d,texCoords,0);
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*c;
		mult		*=persistence;
    }
	// divide by total to get the range -1,1.
	result					*=1.0/total;
	targetTexture8[pos]		=result;
}

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

technique11 simul_random
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, vec4(1.0,1.0,1.0,1.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		//SetBlendState(NoBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,RandomPS()));
    }
}

technique11 simul_noise_2d
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, vec4(1.0,1.0,1.0,1.0 ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		//SetBlendState(NoBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,NoisePS()));
    }
}

technique11 random_3d_compute
{
    pass p0
	{
		SetComputeShader(CompileShader(cs_5_0,CS_Random3D()));
    }
}


technique11 noise_3d_compute
{
    pass p0
	{
		SetComputeShader(CompileShader(cs_5_0,CS_Noise3D()));
    }
}
