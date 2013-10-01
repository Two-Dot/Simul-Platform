#include "CppHLSL.hlsl"
#include "states.hlsl"

#define PATCH_BLEND_BEGIN		800
#define PATCH_BLEND_END			20000

// Shading parameters
cbuffer cbShading : register(b2)
{
	// The color of bottomless water body
	float3		g_WaterbodyColor	: packoffset(c0.x);

	// The strength, direction and color of sun streak
	float		g_Shineness			: packoffset(c0.w);
	float3		g_SunDir			: packoffset(c1.x);
	float3		g_SunColor			: packoffset(c2.x);
	
	// The parameter is used for fixing an artifact
	float3		g_BendParam			: packoffset(c3.x);

	// Perlin noise for distant wave crest
	float		g_PerlinSize		: packoffset(c3.w);
	float3		g_PerlinAmplitude	: packoffset(c4.x);
	float3		g_PerlinOctave		: packoffset(c5.x);
	float3		g_PerlinGradient	: packoffset(c6.x);

	// Constants for calculating texcoord from position
	float		g_TexelLength_x2	: packoffset(c6.w);
	float		g_UVScale			: packoffset(c7.x);
	float		g_UVOffset			: packoffset(c7.y);
};

// Per draw call constants
cbuffer cbChangePerCall : register(b4)
{
	// Transform matrices
	float4x4	g_matLocal;
	float4x4	g_matWorldViewProj;
	float4x4	g_matWorld;

	// Misc per draw call constants
	float2		g_UVBase;
	float2		g_PerlinMovement;
	float3		g_LocalEye;

	// Atmospherics
	float		hazeEccentricity;
	float3		lightDir;
	float4		mieRayleighRatio;
}


//-----------------------------------------------------------------------------------
// Texture & Samplers
//-----------------------------------------------------------------------------------
Texture2D	g_texDisplacement		: register(t0);		// FFT wave displacement map in VS
Texture2D	g_texPerlin				: register(t1);		// FFT wave gradient map in PS
Texture2D	g_texGradient			: register(t2);		// Perlin wave displacement & gradient map in both VS & PS
Texture1D	g_texFresnel			: register(t3);		// Fresnel factor lookup table
TextureCube	g_texReflectCube		: register(t4);		// A small skybox cube texture for reflection

Texture2D	g_skyLossTexture		: register(t5);
Texture2D	g_skyInscatterTexture	: register(t6);
#define PI 3.1415926536f
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

cbuffer cbComputeImmutable : register(b0)
{
	uint g_ActualDim;
	uint g_InWidth;
	uint g_OutWidth;
	uint g_OutHeight;
	uint g_DtxAddressOffset;
	uint g_DtyAddressOffset;
};

cbuffer cbComputePerFrame : register(b1)
{
	float g_Time;
	float g_ChoppyScale;
};

StructuredBuffer<float2>	g_InputH0		: register(t0);
StructuredBuffer<float>		g_InputOmega	: register(t1);
RWStructuredBuffer<float2>	g_OutputHt		: register(u0);


//---------------------------------------- Compute Shaders -----------------------------------------

// Pre-FFT data preparation:

// Notice: In CS5.0, we can output up to 8 RWBuffers but in CS4.x only one output buffer is allowed,
// that way we have to allocate one big buffer and manage the offsets manually. The restriction is
// not caused by NVIDIA GPUs and does not present on NVIDIA GPUs when using other computing APIs like
// CUDA and OpenCL.

// H(0) -> H(t)
[numthreads(BLOCK_SIZE_X, BLOCK_SIZE_Y, 1)]
void UpdateSpectrumCS(uint3 DTid : SV_DispatchThreadID)
{
	int in_index = DTid.y * g_InWidth + DTid.x;
	int in_mindex = (g_ActualDim - DTid.y) * g_InWidth + (g_ActualDim - DTid.x);
	int out_index = DTid.y * g_OutWidth + DTid.x;

	// H(0) -> H(t)
	float2 h0_k  = g_InputH0[in_index];
	float2 h0_mk = g_InputH0[in_mindex];
	float sin_v, cos_v;
	sincos(g_InputOmega[in_index] * g_Time, sin_v, cos_v);

	float2 ht;
	ht.x = (h0_k.x + h0_mk.x) * cos_v - (h0_k.y + h0_mk.y) * sin_v;
	ht.y = (h0_k.x - h0_mk.x) * sin_v + (h0_k.y - h0_mk.y) * cos_v;

	// H(t) -> Dx(t), Dy(t)
	float kx = DTid.x - g_ActualDim * 0.5f;
	float ky = DTid.y - g_ActualDim * 0.5f;
	float sqr_k = kx * kx + ky * ky;
	float rsqr_k = 0;
	if (sqr_k > 1e-12f)
		rsqr_k = 1 / sqrt(sqr_k);
	//float rsqr_k = 1 / sqrtf(kx * kx + ky * ky);
	kx *= rsqr_k;
	ky *= rsqr_k;
	float2 dt_x = float2(ht.y * kx, -ht.x * kx);
	float2 dt_y = float2(ht.y * ky, -ht.x * ky);

    if ((DTid.x < g_OutWidth) && (DTid.y < g_OutHeight))
	{
        g_OutputHt[out_index] = ht;
		g_OutputHt[out_index + g_DtxAddressOffset] = dt_x;
		g_OutputHt[out_index + g_DtyAddressOffset] = dt_y;
	}
}

// FFT wave displacement map in VS, XY for choppy field, Z for height field
SamplerState g_samplerDisplacement	: register(s0)
{
	Filter			= MIN_MAG_MIP_POINT;
	AddressU		= Wrap;
	AddressV		= Wrap;
	AddressW		= Wrap;
	MipLODBias		= 0;
	MaxAnisotropy	= 1;
	//ComparisonFunc	= D3D11_COMPARISON_NEVER;
	MinLOD			= 0;
	MaxLOD			= 1e23;
};

// Perlin noise for composing distant waves, W for height field, XY for gradient
SamplerState g_samplerPerlin		: register(s1)
{
	Filter =ANISOTROPIC;
	AddressU =WRAP;
	AddressV =WRAP;
	AddressW =WRAP;
	MipLODBias = 0;
	MaxAnisotropy = 1;
	//ComparisonFunc = NEVER;
	MinLOD = 0;
	MaxLOD = 1e23;
	MaxAnisotropy = 4;
};

// FFT wave gradient map, converted to normal value in PS
SamplerState g_samplerGradient		: register(s2)
{
	Filter =ANISOTROPIC;
	AddressU =WRAP;
	AddressV =WRAP;
	AddressW =WRAP;
	MipLODBias = 0;
	MaxAnisotropy = 1;
	//ComparisonFunc = NEVER;
	MinLOD = 0;
	MaxLOD = 1e23;
	MaxAnisotropy = 8;
};
// Fresnel factor lookup table
SamplerState g_samplerFresnel		: register(s3)
{
	Filter =MIN_MAG_MIP_LINEAR;
	AddressU =CLAMP;
	AddressV =CLAMP;
	AddressW =CLAMP;
	MipLODBias = 0;
	MaxAnisotropy = 1;
	//ComparisonFunc = NEVER;
	MinLOD = 0;
	MaxLOD = 1e23;
	MaxAnisotropy = 4;
};

// A small sky cubemap for reflection
SamplerState g_samplerCube			: register(s4)
{
	Filter			= MIN_MAG_MIP_LINEAR;
	AddressU		= Wrap;
	AddressV		= Wrap;
	AddressW		= Wrap;
	MipLODBias		= 0;
	MaxAnisotropy	= 1;
	//ComparisonFunc	= D3D11_COMPARISON_NEVER;
	MinLOD			= 0;
	MaxLOD			= 1e23;
};

SamplerState g_samplerAtmospherics	: register(s5)
{
	Filter			= MIN_MAG_MIP_LINEAR;
	AddressU		= Clamp;
	AddressV		= Mirror;
	AddressW		= Clamp;
	MipLODBias		= 0;
	MaxAnisotropy	= 1;
	//ComparisonFunc	= D3D11_COMPARISON_NEVER;
	MinLOD			= 0;
	MaxLOD			= 1e23;
};

//-----------------------------------------------------------------------------
// Name: OceanSurfVS
// Type: Vertex shader                                      
// Desc: Ocean shading vertex shader. Check SDK document for more details
//-----------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD0;
    float3 LocalPos		: TEXCOORD1;
    float2 fade_texc	: TEXCOORD2;
};
#ifndef MAX_FADE_DISTANCE_METRES
	#define MAX_FADE_DISTANCE_METRES (300000.f)
#endif
VS_OUTPUT OceanSurfVS(float2 vPos : POSITION)
{
	VS_OUTPUT Output;

	// Local position
	float4 pos_local = mul(float4(vPos, 0, 1), g_matLocal);
	// UV
	float2 uv_local = pos_local.xy * g_UVScale + g_UVOffset;

	// Blend displacement to avoid tiling artifact
	float3 eye_vec = pos_local.xyz - g_LocalEye;
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor, 0, 1);

	// Add perlin noise to distant patches
	float perlin = 0;
	float2 perlin_tc = uv_local * g_PerlinSize + g_UVBase;
	if (blend_factor < 1)
	{
		float perlin_0 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.x + g_PerlinMovement, 0).w;
		float perlin_1 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.y + g_PerlinMovement, 0).w;
		float perlin_2 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.z + g_PerlinMovement, 0).w;
		
		perlin = perlin_0 * g_PerlinAmplitude.x + perlin_1 * g_PerlinAmplitude.y + perlin_2 * g_PerlinAmplitude.z;
	}

	// Displacement map
	float3 displacement = 0;
	if (blend_factor > 0)
		displacement = g_texDisplacement.SampleLevel(g_samplerDisplacement, uv_local, 0).xyz;
	displacement = lerp(float3(0, 0, perlin), displacement, blend_factor);
	pos_local.xyz += displacement;
pos_local.z+=500.f*g_texPerlin.SampleLevel(g_samplerPerlin,perlin_tc/32.f+ g_PerlinMovement/4.f, 0).w;
	// Transform
	Output.Position = mul(pos_local, g_matWorldViewProj);
   // Output.Position = mul( g_matWorldViewProj,pos_local);
	Output.LocalPos = pos_local.xyz;
	
	// Pass thru texture coordinate
	Output.TexCoord = uv_local;

	float3 wPosition;
	wPosition= mul(pos_local, g_matWorld).xyz;
	
	float3 view=normalize(wPosition.xyz);
#ifdef Y_VERTICAL
	float sine=view.y;
#else
	float sine=view.z;
#endif
	float depth=length(wPosition.xyz)/MAX_FADE_DISTANCE_METRES;
	//OUT.fade_texc=float2(length(OUT.wPosition.xyz)/MAX_FADE_DISTANCE_METRES,0.5f*(1.f-sine));
	Output.fade_texc=float2(sqrt(depth),0.5f*(1.f-sine));
	return Output; 
}

#define pi (3.1415926536f)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

//-----------------------------------------------------------------------------
// Name: OceanSurfPS
// Type: Pixel shader                                      
// Desc: Ocean shading pixel shader. Check SDK document for more details
//-----------------------------------------------------------------------------
float4 OceanSurfPS(VS_OUTPUT In) : SV_Target
{
	// Calculate eye vector.
	float3 eye_vec = g_LocalEye - In.LocalPos;
	float3 eye_dir = normalize(eye_vec);
	// --------------- Blend perlin noise for reducing the tiling artifacts
	// Blend displacement to avoid tiling artifact
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor * blend_factor * blend_factor, 0, 1);

	// Compose perlin waves from three octaves
	float2 perlin_tc = In.TexCoord * g_PerlinSize + g_UVBase;
	float2 perlin_tc0 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.x + g_PerlinMovement : 0;
	float2 perlin_tc1 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.y + g_PerlinMovement : 0;
	float2 perlin_tc2 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.z + g_PerlinMovement : 0;

	float2 perlin_0 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc0).xy;
	float2 perlin_1 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc1).xy;
	float2 perlin_2 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc2).xy;
	
	float2 perlin = (perlin_0 * g_PerlinGradient.x + perlin_1 * g_PerlinGradient.y + perlin_2 * g_PerlinGradient.z);

	// --------------- Water body color
	// Texcoord mash optimization: Texcoord of FFT wave is not required when blend_factor > 1
	float2 fft_tc = (blend_factor > 0) ? In.TexCoord : 0;

	float2 grad = g_texGradient.Sample(g_samplerGradient, fft_tc).xy;
	grad = lerp(perlin, grad, blend_factor);

	// Calculate normal here.
	float3 normal = normalize(float3(grad, g_TexelLength_x2));
	// Reflected ray
	float3 reflect_vec = reflect(-eye_dir, normal);
	// dot(N, V)
	float cos_angle = dot(normal, eye_dir);
	// --------------- Reflected color
	// ramp.x for fresnel term.
	float4 ramp = g_texFresnel.Sample(g_samplerFresnel, cos_angle).xyzw;
// A workaround to deal with "indirect reflection vectors" (which are rays requiring multiple reflections to reach the sky).
//	if (reflect_vec.z < g_BendParam.x)
//		ramp = lerp(ramp, g_BendParam.z, (g_BendParam.x - reflect_vec.z)/(g_BendParam.x - g_BendParam.y));
	reflect_vec.z = max(0, reflect_vec.z);
#ifdef Y_VERTICAL
	float3 reflected_color = g_texReflectCube.Sample(g_samplerCube, reflect_vec.xzy).xyz;
#else
	float3 reflected_color = g_texReflectCube.Sample(g_samplerCube, reflect_vec.xyz).xyz;
#endif
	// Combine waterbody color and reflected color
	float3 water_color = lerp(g_WaterbodyColor, reflected_color, ramp.x);
	// --------------- Sun spots
/*	float cos_spec = clamp(dot(reflect_vec, g_SunDir), 0, 1);
	float sun_spot = pow(cos_spec, g_Shineness);
	water_color += g_SunColor * sun_spot;*/
	float3 loss=g_skyLossTexture.Sample(g_samplerAtmospherics,In.fade_texc).rgb;
	float4 insc=g_skyInscatterTexture.Sample(g_samplerAtmospherics,In.fade_texc);
	float3 inscatter=InscatterFunction(insc,cos_angle);
	//water_color*=loss;
	//water_color+=inscatter;
	return float4(water_color, 1);
}

//-----------------------------------------------------------------------------
// Name: WireframePS
// Type: Pixel shader                                      
// Desc:
//-----------------------------------------------------------------------------
float4 WireframePS() : SV_Target
{
	return float4(0.9f, 0.9f, 0.9f, 1);
}

#ifdef FX
technique11 ocean
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,OceanSurfVS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,OceanSurfPS()));
	}
}
technique11 wireframe
{
    pass p0 
    {
		SetRasterizerState( wireframeRasterizer );
		SetDepthStencilState(TestDepth, 0 );
		SetBlendState(AddBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,OceanSurfVS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,WireframePS()));
	}
}

technique11 update_spectrum
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_4_0,UpdateSpectrumCS()));
	}
}

#endif