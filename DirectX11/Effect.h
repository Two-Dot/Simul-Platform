#pragma once
#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include <array>
#include "DirectXHeader.h"
#include "Platform/DirectX11/ConstantBuffer.h"

#pragma warning(disable:4251)

struct ID3D11Buffer;
#if !PLATFORM_D3D11_SFX
struct ID3DX11EffectConstantBuffer;
struct ID3DX11EffectPass;
#endif
namespace simul
{
	namespace dx11
	{
		struct SIMUL_DIRECTX11_EXPORT Query:public crossplatform::Query
		{
			ID3D11Query *d3d11Query[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				for(int i=0;i<QueryLatency;i++)
					d3d11Query[i]		=0;
			}
			virtual ~Query() override
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;
			void Begin(crossplatform::DeviceContext &deviceContext) override;
			void End(crossplatform::DeviceContext &deviceContext) override;
			bool GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void SetName(const char *n) override;
		};
		struct SIMUL_DIRECTX11_EXPORT RenderState:public crossplatform::RenderState
		{
			ID3D11DepthStencilState		*m_depthStencilState=nullptr;
			ID3D11BlendState			*m_blendState = nullptr;
			ID3D11RasterizerState		* m_rasterizerState = nullptr;
			RenderState();
			virtual ~RenderState();
			void InvalidateDeviceObjects() override;
		};

		class SIMUL_DIRECTX11_EXPORT Shader :public simul::crossplatform::Shader
		{
		public:
#if PLATFORM_D3D11_SFX
			virtual ~Shader();
			void Release() override;
			void load(crossplatform::RenderPlatform* r, const char* filename_utf8, const void* data, size_t len, crossplatform::ShaderType t) override;
			ID3DBlob* shaderBlob=nullptr;
			union
			{
				ID3D11VertexShader* vertexShader = nullptr;
				ID3D11PixelShader* pixelShader;
				ID3D11ComputeShader* computeShader;
			};
#else
			void load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *fileData, size_t DataSize, crossplatform::ShaderType t) override {}
#endif
		};
		class SIMUL_DIRECTX11_EXPORT EffectPass:public simul::crossplatform::EffectPass
		{
		public:
					EffectPass(crossplatform::RenderPlatform *,crossplatform::Effect *);
#if PLATFORM_D3D11_SFX
			void	Apply(crossplatform::DeviceContext &deviceContext,bool test=false) override;

			void	SetConstantBuffers(crossplatform::DeviceContext& context,crossplatform::ConstantBufferAssignmentMap& cBuffers);
			void	SetSamplers(crossplatform::DeviceContext& deviceContext, crossplatform::SamplerStateAssignmentMap& samplers);
			void	SetSRVs(crossplatform::DeviceContext& deviceContext,crossplatform::TextureAssignmentMap& textures, crossplatform::StructuredBufferAssignmentMap& sBuffers);

			void	SetUAVs(crossplatform::DeviceContext& deviceContext, crossplatform::TextureAssignmentMap& rwTextures, crossplatform::StructuredBufferAssignmentMap& sBuffers);
			
		private:
			std::array<ID3D11Buffer*,size_t(32)>	appliedConstantBuffers;
#else
			void Apply(crossplatform::DeviceContext &deviceContext, bool test = false) override;

#endif
		};
		class SIMUL_DIRECTX11_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
		public:
			EffectTechnique(simul::crossplatform::RenderPlatform *r,crossplatform::Effect *e);
			virtual ~EffectTechnique();
			int NumPasses() const;
			crossplatform::EffectPass *AddPass(const char *name,int i) override;
		};
		class SIMUL_DIRECTX11_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
#if !PLATFORM_D3D11_SFX
			ID3DX11EffectPass *currentPass;
#endif
		public:
			Effect();
			virtual ~Effect();
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
#if PLATFORM_D3D11_SFX
			void PostLoad() override;
#endif
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
#if !PLATFORM_D3D11_SFX

			void SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex,int index=-1,int mip=-1);
			
#endif			
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass) override;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass) override;
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext) override;
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
#if PLATFORM_D3D11_SFX
			void SetConstantBuffer(crossplatform::DeviceContext& deviceContext, crossplatform::ConstantBufferBase* s) override;

			void CheckShaderSlots(dx11::Shader* shader, ID3DBlob* shaderBlob);
#endif
		protected:
#if !PLATFORM_D3D11_SFX
			ID3DX11EffectConstantBuffer *constantBuffersBySlot[32];
			ID3DX11EffectConstantBuffer * GetConstantBufferBySlot( uint32_t Slot);
#endif		
		};
	}
}
