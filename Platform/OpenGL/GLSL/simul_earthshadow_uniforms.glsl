layout(std140) uniform EarthShadowUniforms
{
	uniform vec3 earthShadowNormal;
	uniform float radiusOnCylinder;
	uniform float maxFadeDistance;
	uniform float terminatorCosine;
	uniform vec3 sunDir;
};

#ifndef __cplusplus
vec4 EarthShadowFunction(vec2 texc2,vec3 view,float depth)
{
	vec4 insc=texture2D(inscTexture,texc2);
	
	// The Earth's shadow: let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	// First get the part of view that is along the light direction
	float along=dot(sunDir,view);
	float in_shadow=saturate(-along-terminatorCosine);
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*sunDir;
	// Now get the part that's on the cylinder radius:
	float on_radius=dot(on_cross_section,earthShadowNormal);
	vec3 on_x=on_cross_section-on_radius*earthShadowNormal;
	float sine_phi=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float cos2=1.0-sine_phi*sine_phi;
	float r=radiusOnCylinder;//min(radiusOnCylinder,1.0);
	// Normalized so that Earth radius is 1.0..
	float u=1.0-r*r*cos2;
	float d=0.0;
	if(u>=0.0)
	{
		float L=-r*sine_phi;
		if(r<=1.0)
			L+=sqrt(u);
		else
			L-=sqrt(u);
		L=max(0.0,L);
		float sine_gamma=length(on_cross_section);
		L=L/sine_gamma;
		// L is the distance to the outside of the Earth's shadow in this direction, normalized to the Earth's radius.
		// Renormalize it to the fade distance.
		d=L/maxFadeDistance;
	}
	d=min(d,depth);
	// Inscatter at distance d
	vec2 texcoord_d=vec2(sqrt(d),texc2.y);
	vec4 inscb=texture(inscTexture,texcoord_d);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	// but we just use the inscatter to d if we're looking INTO the cylinder.
	if(r<=1.0||d==0.0)
	{
		insc-=inscb*saturate(in_shadow);
    }
	else
	{
	// but we just use the inscatter to d if we're looking INTO the cylinder.
		insc=mix(insc,inscb,in_shadow);
    }
   return insc;
}
#endif