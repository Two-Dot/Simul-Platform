#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/rain_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
texture2D randomTexture;
TextureCube cubeTexture;
texture2D rainTexture;
texture2D showTexture;
SamplerState rainSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct vertexInputRenderRainTexture
{
    float4 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexInput
{
    float3 position		: POSITION;
    float4 texCoords	: TEXCOORD0;
};

struct posOnly
{
    float3 position		: POSITION;
	uint vertex_id			: SV_VertexID;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
    float4 texCoords	: TEXCOORD0;		/// z is intensity!
    float3 viewDir		: TEXCOORD1;
};

float rand(float2 co)
{
    return frac(sin(dot(co.xy ,float2(12.9898,78.233))) * 43758.5453);
}

struct rainVertexOutput
{
    float4 position			: SV_POSITION;
    float2 pos				: TEXCOORD1;
    float2 texCoords		: TEXCOORD0;
};

struct particleVertexOutput
{
    float4 position			:SV_POSITION;
	float pointSize			:PSIZE;
	float brightness		:TEXCOORD0;
	vec3 view				:TEXCOORD1;
};

struct particleGeometryOutput
{
    float4 position			:SV_POSITION;
    float2 texCoords		:TEXCOORD0;
	float brightness		:TEXCOORD1;
	vec3 view				:TEXCOORD2;
};

vec3 Frac(vec3 pos,float scale)
{
	vec3 unity		=vec3(1.0,1.0,1.0);
	return scale*(2.0*frac(0.5*(pos/scale+unity))-unity);
}

posTexVertexOutput VS_ShowTexture(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}


float4 PS_ShowTexture(posTexVertexOutput In): SV_TARGET
{
    return texture_wrap_lod(showTexture,In.texCoords,0);
}

particleVertexOutput VS_Particles(posOnly IN)
{
	particleVertexOutput OUT;
	vec3 particlePos	=IN.position.xyz;
	particlePos			*=30.0;
	particlePos			-=viewPos;
	particlePos.z		-=offset;
	particlePos			=Frac(particlePos,30.0);
	float p				=flurryRate*phase;
	particlePos			+=.125*flurry*randomTexture.SampleLevel(wrapSamplerState,vec2(2.0*p+1.7*IN.position.x,2.0*p+2.3*IN.position.y),0).xyz;
	particlePos			+=flurry*randomTexture.SampleLevel(wrapSamplerState,vec2(p+IN.position.x,p+IN.position.y),0).xyz;
	OUT.position		=mul(worldViewProj,float4(particlePos.xyz,1.0));
	OUT.view			=normalize(particlePos.xyz);
	OUT.pointSize		=snowSize;
	OUT.brightness		=1.f;//(float)frac(viewPos.x);//60.1/length(pos-viewPos);
	return OUT;
}

cbuffer cbImmutable
{
    float4 g_positions[4] : packoffset(c0) =
    {
        float4( 0.5,-0.5,0,0),
        float4( 0.5, 0.5,0,0),
        float4(-0.5,-0.5,0,0),
        float4(-0.5, 0.5,0,0),
    };
    float2 g_texcoords[4] : packoffset(c4) = 
    { 
        float2(1,0), 
        float2(1,1),
        float2(0,0),
        float2(0,1),
    };
};

[maxvertexcount(4)]
void GS_Particles(point particleVertexOutput input[1], inout TriangleStream<particleGeometryOutput> SpriteStream)
{
    particleGeometryOutput output;
	// Emit two new triangles
	[unroll]for(int i=0; i<4; i++)
	{
        output.position = input[0].position + float4(g_positions[i].xy * input[0].pointSize,0,0); 
        //output.Position.y *= g_fAspectRatio; // Correct for the screen aspect ratio, since the sprite is calculated in eye space
		output.texCoords	=g_texcoords[i];
        output.brightness	=input[0].brightness;  
        output.view			=input[0].view;     
		SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}

float4 PS_Particles(particleGeometryOutput IN): SV_TARGET
{
	float4 result	=cubeTexture.Sample(wrapSamplerState,IN.view);
	vec2 pos		=IN.texCoords*2.0-1.0;
	float radius	=intensity*length(pos.xy);
	float opacity	=saturate(intensity-radius)/.5;
	return float4(IN.brightness*lightColour.rgb+result.rgb,opacity);
}

rainVertexOutput VS_FullScreen(idOnly IN)
{
	rainVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.pos			=poss[IN.vertex_id];
	OUT.position	=float4(OUT.pos,0.0,1.0);
	// Set to far plane
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.pos.x,-OUT.pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

float4 PS_RenderRainTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
	float2 t=IN.texCoords.xy;
	for(int i=0;i<32;i++)
	{
		r+=rand(frac(t.xy));
		t.y+=1.0/512.0;
	}
	r/=32.0;
	float4 result=float4(r,r,r,r);
    return result;
}

float4 PS_RenderRandomTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
    vec4 result=vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords));
    return result;
}

float4 PS_Overlay(rainVertexOutput IN) : SV_TARGET
{
	float3 view		=normalize(mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz);
	float3 light	=cubeTexture.Sample(wrapSamplerState,-view).rgb;
	float sc=1.0;
	float br=1.0;
	vec4 result=vec4(light.rgb,0);//float4(0.0,0.0,0.0,0.0);
	for(int i=0;i<4;i++)
	{
		float2 texc	=float2(atan2(view.x,view.y)*sc*7/(2.0*pi),(view.z+sc*offset)*sc);
		float r		=br*(saturate(rainTexture.Sample(wrapSamplerState,texc.xy)).x+intensity-1.0);//
		sc*=4.0;
		br*=.4;
		//result*=1.0-r;
		//result.rgb+=r*light.rgb;
		result.a+=r;
	}
	result.a=saturate(result.a);
	return result;
}

technique11 simul_rain
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_Overlay()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 simul_particles
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
        SetGeometryShader(CompileShader(gs_4_0,GS_Particles()));
		SetVertexShader(CompileShader(vs_4_0,VS_Particles()));
		SetPixelShader(CompileShader(ps_4_0,PS_Particles()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 create_rain_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRainTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 create_random_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRandomTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique11 show_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_ShowTexture()));
		SetPixelShader(CompileShader(ps_4_0,PS_ShowTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
