#ifndef GODRAYS_SL
#define GODRAYS_SL

vec2 GetIlluminationAt2(Texture2D cloudShadowTexture,vec3 texc)
{
	vec4 texel			=texture_cwc_lod(cloudShadowTexture,texc.xy,0);
	vec2 illumination	=texel.xy;
	float above			=saturate((texc.z-texel.z)/0.5);
	illumination		+=above;
	return saturate(illumination);
}

vec4 FastGodrays(Texture2D cloudGodraysTexture,Texture2D inscTexture,Texture2D overcTexture,vec2 pos,mat4 invViewProj,float maxFadeDistance,float solid_dist)
{
	vec3 view				=mul(invViewProj,vec4(pos.xy,1.0,1.0)).xyz;
	view					=normalize(view);
	// Get the direction in shadow space, then scale so that we have unit xy.
	vec3 view_s				=normalize(mul(invShadowMatrix,vec4(view,0.0)).xyz);
	vec3 tex_pos			=mul(invShadowMatrix,vec4(view,0.0)).xyz;
	float azimuth_texc		=atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536);
	float true_distance		=solid_dist*maxFadeDistance;
	float dist_texc			=length(view_s.xy)/length(view_s)*true_distance/shadowRange;
	vec4 illum_amount		=texture_wrap_clamp(cloudGodraysTexture,vec2(azimuth_texc,dist_texc));
    //illum_amount/=dist_texc;
	float sine				=view.z;
	float cos0				=dot(view,lightDir);
	vec2 solid_fade_texc	=vec2(pow(solid_dist,0.5),0.5*(1.0-sine));
	vec4 insc_s				=texture_clamp_mirror(inscTexture,solid_fade_texc)
								-texture_clamp_mirror(overcTexture,solid_fade_texc);
	vec4 extra_insc			=insc_s*saturate(illum_amount.x);
	vec3 gr					=InscatterFunction(extra_insc,hazeEccentricity,cos0,mieRayleighRatio);
	gr						*=exposure;
    return vec4(gr,1.0);
}

	
#endif
