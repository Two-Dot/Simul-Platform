#include "CppHlsl.hlsl"
#include "../../CrossPlatform/SL/render_states.sl"
#include "../../CrossPlatform/SL/lightning_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/render_states.sl"

Texture2D lightningTexture;
Texture2D depthTexture;
Texture2DMS<float4> depthTextureMS;
Texture2D cloudDepthTexture;

struct transformedVertex
{
    vec4 hPosition	: SV_POSITION;
	vec4 texCoords	: TEXCOORD0;
    vec2 hPosCentre1: TEXCOORD1;
    vec2 hPosCentre2: TEXCOORD2;
	vec2 screenPos:	TEXCOORD3;
	vec2 texc		:	TEXCOORD4;
    float along: TEXCOORD5;
	float width: TEXCOORD6;
    float depth	: TEXCOORD7;
};

struct transformedThinVertex
{
    vec4 hPosition	: SV_POSITION;
	vec4 texCoords	: TEXCOORD0;
    float depth	: TEXCOORD1;
	vec2 screenPos:	TEXCOORD2;
	vec2 texc		:	TEXCOORD3;
};

posTexVertexOutput VS_FullScreen(idOnly IN)
{
    posTexVertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#if REVERSE_DEPTH==1
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
	return OUT;
}

transformedThinVertex VS_Thin(LightningVertexInput IN)
{
    transformedThinVertex OUT;
    OUT.hPosition	=mul(worldViewProj, vec4(IN.position.xyz , 1.0));
	OUT.texCoords	=IN.texCoords;
	OUT.depth		=OUT.hPosition.z/OUT.hPosition.w;
	OUT.screenPos	=OUT.hPosition.xy/OUT.hPosition.w;
	OUT.texc		=OUT.screenPos.xy*0.5+0.5;
    return OUT;
}

LightningVertexOutput VS_Thick(LightningVertexInput IN)
{
    LightningVertexOutput OUT;
    OUT.position	=mul(worldViewProj, vec4(IN.position.xyz , 1.0));
	OUT.texCoords	=IN.texCoords;
	OUT.depth		=OUT.position.z/OUT.position.w;
    return OUT;
}

vec2 PixelPos(vec4 vertex)
{
	return vec2(vertex.xy/vertex.w)*viewportPixels;
}

[maxvertexcount(10)]
void GS_Thick(lineadj LightningVertexOutput input[4], inout TriangleStream<transformedVertex> SpriteStream)
{
	transformedVertex output;
    //  a - - - - - - - - - - - - - - - - b
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  | - - -start - - - - - - end- - - |
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  d - - - - - - - - - - - - - - - - c
	vec2 p0		=PixelPos(input[0].position);
	vec2 p1		=PixelPos(input[1].position);
	vec2 p2		=PixelPos(input[2].position);
	vec2 p3		=PixelPos(input[3].position);
	vec2 area	=viewportPixels * 1.2;
	if(p1.x<-area.x||p1.x>area.x) return;
	if(p1.y<-area.y||p1.y>area.y) return;
	if(p2.x<-area.x||p2.x>area.x) return;
	if(p2.y<-area.y||p2.y>area.y) return;
	if(input[0].position.z<0) return;
	if(input[0].position.z>1.0) return;
    vec4 start			=input[1].position;
    vec4 end			=input[2].position;
	// determine the direction of each of the 3 segments (previous, current, next
	vec2 v0				=normalize(p1-p0);
	vec2 v1				=normalize(p2-p1);
	vec2 v2				=normalize(p3-p2);
	// determine the normal of each of the 3 segments (previous, current, next)
	vec2 n0				=vec2(-v0.y,v0.x);
	vec2 n1				=vec2(-v1.y,v1.x);
	vec2 n2				=vec2(-v2.y,v2.x);
	// determine miter lines by averaging the normals of the 2 segments
	vec2 miter_a		=normalize(n0 + n1);	// miter at start of current segment
	vec2 miter_b		=normalize(n1 + n2);	// miter at end of current segment
	// determine the length of the miter by projecting it onto normal and then inverse it
	float width1		=input[1].texCoords.x;
	float width2		=input[2].texCoords.x;
	float lengthPixels_a		=width1/start.w	*viewportPixels.x/dot(miter_a, n1);
	float lengthPixels_b		=width2/end.w	*viewportPixels.x/dot(miter_b, n1);
	const float	MITER_LIMIT=1.0;
	output.hPosCentre1	=vec2(p1.xy/viewportPixels);
	output.hPosCentre2	=vec2(p2.xy/viewportPixels);
	output.width		=width1/start.w;
	vec2 diff	=output.hPosCentre2-output.hPosCentre1;
	float dist	=length(diff);
	float d2	=dist*dist;
	// prevent excessively long miters at sharp corners
	if( dot(v0,v1) < -MITER_LIMIT )
	{
		miter_a = n1;
		lengthPixels_a = width1;
		// close the gap
		if(dot(v0,n1)>0)
		{
			output.hPosition	=vec4( (p1 + width1 * n0) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			output.hPosition	=vec4( (p1 + width1 * n1) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			output.hPosition	=vec4( p1 / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0.5,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			SpriteStream.RestartStrip();
		}
		else
		{
			output.hPosition	=vec4( (p1 - width2 * n1) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			output.hPosition	=vec4( (p1 - width2 * n0) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			output.hPosition	=vec4( p1 / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy/output.hPosition.w;
			output.texc			=output.screenPos.xy*0.5+0.5;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0.5,input[1].texCoords.yzw);
			output.depth		=input[1].depth;
			SpriteStream.Append(output);
			SpriteStream.RestartStrip();
		}
	}
	if( dot(v1,v2) < -MITER_LIMIT )
	{
		miter_b = n1;
		lengthPixels_b = width2;
	}
  // generate the triangle strip
	output.width		=width1/start.w;
	output.screenPos	=(p1 + lengthPixels_a * miter_a)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*start.w,start.z,start.w);
	output.texc			=output.screenPos.xy*0.5+0.5;
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2*1.1-0.05;
	output.texCoords	=vec4(0.0,input[1].texCoords.yzw);

	output.depth		=input[1].depth;
	SpriteStream.Append(output);
	output.screenPos	=(p1 - lengthPixels_a * miter_a)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*start.w,start.z,start.w);
	output.texc			=output.screenPos.xy*0.5+0.5;
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
	output.depth		=input[1].depth;
	SpriteStream.Append(output);
	output.width		=width2/end.w;
	output.screenPos	=(p2 + lengthPixels_b * miter_b)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*end.w,end.z,end.w);
	output.texc			=output.screenPos.xy*0.5+0.5;
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(0.0,input[2].texCoords.yzw);
	output.depth		=input[2].depth;
	SpriteStream.Append(output);
	output.screenPos	=(p2 - lengthPixels_b * miter_b)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*end.w,end.z,end.w);
	output.texc			=output.screenPos.xy*0.5+0.5;
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(1.0,input[2].texCoords.yzw);
	output.depth		=input[2].depth;
	SpriteStream.Append(output);
    SpriteStream.RestartStrip();
}
vec4 PS_Test(posTexVertexOutput IN): SV_TARGET
{
	vec4 dlookup 		=texture_wrap(depthTexture,IN.texCoords.xy);
	vec2 dist			=depthToFadeDistance(dlookup.xy,vec2(0,0),depthToLinFadeDistParams,tanHalfFov);
	return vec4(dist,dlookup.z,0.5);
}
float4 PS_Main(transformedVertex IN): SV_TARGET
{
	vec2 texc=vec2(IN.texc.x,1.0-IN.texc.y);
	vec4 dlookup 		=texture_wrap(depthTexture,texc.xy);
	vec4 clookup 		=texture_wrap(cloudDepthTexture,texc.xy);

	vec4 clip_pos		=vec4(IN.screenPos,1.0,1.0);
#if REVERSE_DEPTH==1
	float depth			=max(dlookup.x,clookup.x);
#else
	float depth			=min(dlookup.x,clookup.x);
#endif
	vec2 dist			=depthToFadeDistance(vec2(depth,IN.depth),IN.screenPos.xy,depthToLinFadeDistParams,tanHalfFov);
	//return vec4(100*dist.xy,100*dlookup.z,1.0);//dlookup.rb,1);
	
	float b			=2.0*(IN.texCoords.x-0.5);
	float br		=IN.texCoords.w;//pow(1.0-b*b,4.0);// w is the local brightness factor
	vec2 centre		=lerp(IN.hPosCentre1,IN.hPosCentre2,saturate(IN.along));
	vec2 diff		=IN.screenPos-centre;
	float d			=length(diff)/IN.width;
//	br				*=exp(-2.0*d);
	if(br<0)
		br=0;
	float4 colour	=br*lightningColour;//lightningTexture.Sample(clampSamplerState,IN.texCoords.xy);
	colour			*=saturate(1.0-(dist.y-dist.x)/0.001);
	
    return colour;
}

float4 PS_Thin(transformedThinVertex IN): SV_TARGET
{
	vec2 texc=vec2(IN.texc.x,1.0-IN.texc.y);
	vec4 dlookup 		=texture_clamp(depthTexture,texc.xy);
	vec4 clookup 		=texture_clamp(cloudDepthTexture,texc.xy);
#if REVERSE_DEPTH==1
	float depth			=max(dlookup.x,clookup.x);
#else
	float depth			=min(dlookup.x,clookup.x);
#endif
	vec2 dist			=depthToFadeDistance(vec2(depth,IN.depth),IN.screenPos.xy,depthToLinFadeDistParams,tanHalfFov);
	//if(dist.x<dist.y)
	//	discard;
	
	float4 colour=lightningColour*IN.texCoords.w;//lightningTexture.Sample(clampSamplerState,IN.texCoords.xy);
	colour			*=saturate(1.0-(dist.y-dist.x)/0.001);
    return colour;
}

technique11 lightning_thick
{
    pass p0 
    {
		SetDepthStencilState(TestDepth,0);//
        SetRasterizerState(RenderNoCull);
		SetBlendState(AddBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Thick()));
        SetGeometryShader( CompileShader(gs_5_0, GS_Thick()));
		SetPixelShader(CompileShader(ps_5_0,PS_Main()));
    }
}
RasterizerState lightningLineRasterizer
{
	FillMode					= WIREFRAME;
	CullMode					= none;
	FrontCounterClockwise		= false;
	DepthBias					= 0;//DEPTH_BIAS_D32_FLOAT(-0.00001);
	DepthBiasClamp				= 0.f;
	SlopeScaledDepthBias		= 0.f;
	DepthClipEnable				= false;
	ScissorEnable				= false;
	MultisampleEnable			= true;
	AntialiasedLineEnable		= false;
};

technique11 lightning_thin
{
    pass p0 
    {
		SetDepthStencilState(TestDepth,0);
        SetRasterizerState(lightningLineRasterizer);
		SetBlendState(AddBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Thin()));
		SetPixelShader(CompileShader(ps_5_0,PS_Thin()));
    }
}


technique11 test
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Test()));
    }
}
