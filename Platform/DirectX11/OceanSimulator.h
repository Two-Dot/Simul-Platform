
#ifndef _OCEAN_WAVE_H
#define _OCEAN_WAVE_H

#include <D3DX9.h>
#include <D3D11.h>

#include "Simul/Terrain/BaseSeaRenderer.h"
#include "CSFFT/fft_512x512.h"

//#define CS_DEBUG_BUFFER
#define PAD16(n) (((n)+15)/16*16)

class OceanSimulator
{
public:
	OceanSimulator(simul::terrain::OceanParameter *params);
	~OceanSimulator();

	void RestoreDeviceObjects(ID3D11Device* pd3dDevice);
	void InvalidateDeviceObjects();
	// -------------------------- Initialization & simulation routines ------------------------
	// Update ocean wave when tick arrives.
	void updateDisplacementMap(float time);

	// Texture access
	ID3D11ShaderResourceView* getDisplacementMap();
	ID3D11ShaderResourceView* getGradientMap();

	const simul::terrain::OceanParameter *getParameters();


protected:
	simul::terrain::OceanParameter *m_param;

	// ---------------------------------- GPU shading assets -----------------------------------
	// D3D objects
	ID3D11DeviceContext			*m_pd3dImmediateContext;
	ID3DX11Effect				*effect;
	
	// Displacement map
	ID3D11Texture2D				*m_pDisplacementMap;		// (RGBA32F)
	ID3D11ShaderResourceView	*m_pDisplacementSRV;
	ID3D11RenderTargetView		*m_pDisplacementRTV;

	// Gradient field
	ID3D11Texture2D				*m_pGradientMap;			// (RGBA16F)
	ID3D11ShaderResourceView	*m_pGradientSRV;
	ID3D11RenderTargetView		*m_pGradientRTV;

	// Samplers
	ID3D11SamplerState			*m_pPointSamplerState;

	// Initialize the vector field.
	void initHeightMap(D3DXVECTOR2* out_h0, float* out_omega);

	// ----------------------------------- CS simulation data ---------------------------------
	// Initial height field H(0) generated by Phillips spectrum & Gauss distribution.
	ID3D11Buffer* m_pBuffer_Float2_H0;
	ID3D11UnorderedAccessView* m_pUAV_H0;
	ID3D11ShaderResourceView* m_pSRV_H0;

	// Angular frequency
	ID3D11Buffer* m_pBuffer_Float_Omega;
	ID3D11UnorderedAccessView* m_pUAV_Omega;
	ID3D11ShaderResourceView* m_pSRV_Omega;

	// Height field H(t), choppy field Dx(t) and Dy(t) in frequency domain, updated each frame.
	ID3D11Buffer* m_pBuffer_Float2_Ht;
	ID3D11UnorderedAccessView* m_pUAV_Ht;
	ID3D11ShaderResourceView* m_pSRV_Ht;

	// Height & choppy buffer in the space domain, corresponding to H(t), Dx(t) and Dy(t)
	ID3D11Buffer* m_pBuffer_Float_Dxyz;
	ID3D11UnorderedAccessView* m_pUAV_Dxyz;
	ID3D11ShaderResourceView* m_pSRV_Dxyz;

	ID3D11Buffer* m_pQuadVB;

	// Shaders, layouts and constants
	ID3D11ComputeShader* m_pUpdateSpectrumCS;

	ID3D11VertexShader* m_pQuadVS;
	ID3D11PixelShader* m_pUpdateDisplacementPS;
	ID3D11PixelShader* m_pGenGradientFoldingPS;

	ID3D11InputLayout* m_pQuadLayout;

	ID3D11Buffer* m_pImmutableCB;
	ID3D11Buffer* m_pPerFrameCB;

	// FFT wrap-up
	FFT_512x512 m_fft;

#ifdef CS_DEBUG_BUFFER
	ID3D11Buffer* m_pDebugBuffer;
#endif
};

#endif	// _OCEAN_WAVE_H
