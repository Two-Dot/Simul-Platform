
#ifndef _OCEAN_WAVE_H
#define _OCEAN_WAVE_H

#ifndef SIMUL_WIN8_SDK
#include <D3DX9.h>
#endif
#include <D3D11.h>

#include "Simul/Terrain/BaseSeaRenderer.h"
#include "CSFFT/fft_512x512.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"

//#define CS_DEBUG_BUFFER
#define PAD16(n) (((n)+15)/16*16)

class OceanSimulator
{
public:
	OceanSimulator(simul::terrain::SeaKeyframer *s);
	~OceanSimulator();

	void RestoreDeviceObjects(ID3D11Device* pd3dDevice);
	void RecompileShaders();
	void InvalidateDeviceObjects();
	// -------------------------- Initialization & simulation routines ------------------------
	// Update ocean wave when tick arrives.
	void updateDisplacementMap(float time);

	// Texture access
	ID3D11ShaderResourceView* GetFftInput();
	ID3D11ShaderResourceView* GetFftOutput();
	ID3D11ShaderResourceView* getDisplacementMap();
	ID3D11ShaderResourceView* GetSpectrum();
	ID3D11ShaderResourceView* getGradientMap();

	const simul::terrain::SeaKeyframer *GetSeaKeyframer();

protected:

	simul::terrain::SeaKeyframer *m_param;

	// ---------------------------------- GPU shading assets -----------------------------------
	// D3D objects
	ID3D11Device				*m_pd3dDevice;
	ID3D11DeviceContext			*m_pd3dImmediateContext;
	ID3DX11Effect				*effect;
	
	// Displacement map
	simul::dx11::Texture displacement;

	// Gradient field
	simul::dx11::Texture gradient;

	// Initialize the vector field.
	void initHeightMap(D3DXVECTOR2* out_h0, float* out_omega);

	// ----------------------------------- CS simulation data ---------------------------------
	// Initial height field H(0) generated by Phillips spectrum & Gauss distribution.
	simul::dx11::StructuredBuffer<vec2>				h0;

	// Angular frequency
	simul::dx11::StructuredBuffer<float>				omega;

	// Height field H(t), choppy field Dx(t) and Dy(t) in frequency domain, updated each frame.
	simul::dx11::StructuredBuffer<vec2>				choppy;
	simul::dx11::ConstantBuffer<cbImmutable>		immutableConstants;
	simul::dx11::ConstantBuffer<cbChangePerFrame>	changePerFrameConstants;

	// Height & choppy buffer in the space domain, corresponding to H(t), Dx(t) and Dy(t)
	simul::dx11::StructuredBuffer<vec2>				dxyz;

	// FFT wrap-up
	Fft m_fft;
	float start_time;
};

#endif	// _OCEAN_WAVE_H
