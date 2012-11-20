
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;

uniform float hazeEccentricity;
uniform vec3 lightDir;
uniform mat4 invViewProj;
uniform vec3 mieRayleighRatio;
uniform float directLightMultiplier;
uniform vec3 earthShadowNormal;
uniform float radiusOnCylinder;
uniform float maxFadeDistance;

varying vec2 texCoords;

const float pi=3.1415926536;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

void main()
{
    vec4 lookup=texture2D(imageTexture,texCoords);
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(invViewProj*pos).xyz;
	view=normalize(view);
	float sine=view.z;
	float depth=lookup.a;
	if(depth>=1.0) 
		discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture2D(lossTexture,texc2).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
	vec4 insc=texture2D(inscTexture,texc2);
	
	
	// The Earth's shadow: let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	// First get the part of view that is along the light direction
	float along=dot(lightDir,view);
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*lightDir;
	// Now get the part that's on the cylinder radius:
	float on_radius=dot(on_cross_section,earthShadowNormal);
	vec3 on_x=on_cross_section-on_radius*earthShadowNormal;
	float sine_phi=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float s=sine_phi;
	float cos2=1.0-s*s;
	// Normalized so that Earth radius is 1.0..
	float u=1.0-radiusOnCylinder*radiusOnCylinder*cos2;
	float d=0.0;
	if(u>=0)
	{
		float L=-radiusOnCylinder*sine_phi;
		if(radiusOnCylinder<1.0)
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
	vec2 texcoord_d=vec2(pow(d,0.5),0.5*(1.0-sine));
	vec4 inscb=texture2D(inscTexture,texcoord_d);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	if(radiusOnCylinder<1.0||d==0.0)
	{
		insc-=inscb;
	}
	else
	// but we just use the inscatter to d if we're looking INTO the cylinder.
	{
		insc=mix(insc,inscb,saturate(-along));
	}

	float cos0=dot(view,lightDir);
	colour+=directLightMultiplier*InscatterFunction(insc,cos0);
	vec4 skyl=texture2D(skylightTexture,texc2);
	colour.rgb+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
