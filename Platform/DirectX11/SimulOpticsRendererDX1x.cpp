
#include "SimulOpticsRendererDX1x.h"
#include "MacrosDX1x.h"
#include "CreateEffectDX1x.h"
#include "Simul/Math/Decay.h"

#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11/Utilities.h"
using namespace simul::dx11;

SimulOpticsRendererDX1x::SimulOpticsRendererDX1x(simul::base::MemoryInterface *m)
	:BaseOpticsRenderer(m)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,flare_texture(NULL)
	,rainbowLookupTexture(NULL)
	,coronaLookupTexture(NULL)
{
	FlareTexture=("SunFlare.png");
}

SimulOpticsRendererDX1x::~SimulOpticsRendererDX1x()
{
	InvalidateDeviceObjects();
}

void SimulOpticsRendererDX1x::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	SAFE_RELEASE(flare_texture);
	flare_texture=simul::dx11::LoadTexture(m_pd3dDevice,FlareTexture.c_str());
	
	opticsConstants.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	halo_textures.clear();
	int num_halo_textures=lensFlare.GetNumArtifactTypes();
	halo_textures.resize(num_halo_textures);
	for(int i=0;i<num_halo_textures;i++)
	{
		std::string tn=(lensFlare.GetTypeName(i));
		ID3D1xShaderResourceView* &tex=halo_textures[i];
		SAFE_RELEASE(tex);
		tex=simul::dx11::LoadTexture(m_pd3dDevice,(tn+".png").c_str());
	}
	SAFE_RELEASE(rainbowLookupTexture);
	SAFE_RELEASE(coronaLookupTexture);
	rainbowLookupTexture=simul::dx11::LoadTexture(m_pd3dDevice,"rainbow_scatter.png");
	coronaLookupTexture=simul::dx11::LoadTexture(m_pd3dDevice,"rainbow_diffraction_i_vs_a.png");

	RecompileShaders();
}

void SimulOpticsRendererDX1x::InvalidateDeviceObjects()
{
	opticsConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(effect);
	SAFE_RELEASE(flare_texture);
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_RELEASE(halo_textures[i]);
	}
	SAFE_RELEASE(rainbowLookupTexture);
	SAFE_RELEASE(coronaLookupTexture);
	halo_textures.clear();
}

void SimulOpticsRendererDX1x::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(effect);
	std::map<std::string,std::string> defines;
	HRESULT hr;
	hr=CreateEffect(m_pd3dDevice,&effect,"optics.fx",defines);
	V_CHECK(hr);
	m_hTechniqueFlare			=effect->GetTechniqueByName("simul_flare");
	techniqueRainbowCorona		=effect->GetTechniqueByName("rainbow_and_corona");
	opticsConstants.LinkToEffect(effect,"OpticsConstants");
}

void SimulOpticsRendererDX1x::RenderFlare(void *context,float exposure,const float *dir,const float *light)
{
	HRESULT hr=S_OK;
	if(!effect)
		return;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	StoreD3D11State(pContext);
	D3DXVECTOR3 sun_dir(dir);
	float magnitude=exposure;
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
	//float alt_km=0.001f*cam_pos.y;
	simul::sky::float4 sunlight(light);//=skyKeyframer->GetLocalIrradiance(alt_km)*lensFlare.GetStrength();
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.05f;
	D3DXVECTOR3 cam_pos,cam_dir;
	simul::dx11::GetCameraPosVector(view,(float*)&cam_pos,(float*)&cam_dir,false);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	flare_magnitude*=lensFlare.GetStrength();
	sunlight*=sun_mult*flare_magnitude;
	if(flare_magnitude>0.f)
	{
		dx11::setTexture(effect,"flareTexture",flare_texture);
		SetOpticsConstants(opticsConstants,view,proj,dir,sunlight,flare_angular_size*flare_magnitude);
		opticsConstants.Apply(pContext);
		ApplyPass(pContext,m_hTechniqueFlare->GetPassByIndex(0));
		UtilityRenderer::DrawQuad(pContext);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			D3DXVECTOR3 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
			dx11::setTexture(effect,"flareTexture",halo_textures[t]);
			SetOpticsConstants(opticsConstants,view,proj,pos,sunlight,flare_angular_size*sz*flare_magnitude);
			opticsConstants.Apply(pContext);
			ApplyPass(pContext,m_hTechniqueFlare->GetPassByIndex(0));
			UtilityRenderer::DrawQuad(pContext);
		}
	}
	pContext->VSSetShader(NULL, NULL, 0);
	pContext->GSSetShader(NULL, NULL, 0);
	pContext->PSSetShader(NULL, NULL, 0);
	RestoreD3D11State(pContext );
	RenderRainbowAndCorona(context,exposure,dir,light);
}
void SimulOpticsRendererDX1x::RenderRainbowAndCorona(void *context,float exposure,const float *dir_to_sun,const float *light)
{
	HRESULT hr=S_OK;
	if(!effect)
		return;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	StoreD3D11State(pContext);
	float magnitude=exposure;
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
	simul::sky::float4 sunlight(light);
	D3DXVECTOR3 cam_pos,cam_dir;
	simul::dx11::GetCameraPosVector(view,(float*)&cam_pos,(float*)&cam_dir,false);
	dx11::setTexture(effect,"rainbowLookupTexture"	,rainbowLookupTexture);
	dx11::setTexture(effect,"coronaLookupTexture"	,coronaLookupTexture);
	//dx11::setTexture(effect,"moistureTexture"		,moistureTexture);
	SetOpticsConstants(opticsConstants,view,proj,dir_to_sun,sunlight,flare_angular_size*flare_magnitude);
	opticsConstants.Apply(pContext);
	ApplyPass(pContext,techniqueRainbowCorona->GetPassByIndex(0));
	UtilityRenderer::DrawQuad(pContext);
	pContext->VSSetShader(NULL,NULL,0);
	pContext->GSSetShader(NULL,NULL,0);
	pContext->PSSetShader(NULL,NULL,0);
	RestoreD3D11State(pContext );
}

void SimulOpticsRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}