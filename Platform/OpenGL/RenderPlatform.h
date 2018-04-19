#pragma once

#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

#include <GL/glew.h>

namespace simul
{
	namespace opengl
	{
		class Material;
		//! OpenGL renderplatform implementation
		class SIMUL_OPENGL_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual~ RenderPlatform();
			const char* GetName() const
			{
				return "OpenGL";
			}
			void RestoreDeviceObjects(void*) override;
			void InvalidateDeviceObjects() override;
			void StartRender(crossplatform::DeviceContext &deviceContext);
			void EndRender(crossplatform::DeviceContext &deviceContext);
			void SetReverseDepth(bool);
			void IntializeLightingEnvironment(const float pAmbientLight[3]);
			void DispatchCompute	(crossplatform::DeviceContext& deviceContext,int w,int l,int d);
			void ApplyShaderPass	(crossplatform::DeviceContext& deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int);
			void Draw				(crossplatform::DeviceContext& deviceContext,int num_verts,int start_vert);
			void DrawIndexed		(crossplatform::DeviceContext& deviceContext,int num_indices,int start_index=0,int base_vertex=0);
			void DrawMarker			(crossplatform::DeviceContext& deviceContext,const double *matrix);
			void DrawLine			(crossplatform::DeviceContext& deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			void DrawCrossHair		(crossplatform::DeviceContext& deviceContext,const double *pGlobalPosition);
			void DrawCamera			(crossplatform::DeviceContext& deviceContext,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop		(crossplatform::DeviceContext& deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void DrawTexture        (crossplatform::DeviceContext& deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend = false, float gamma = 1.0f, bool debug = false);
			void DrawQuad			(crossplatform::DeviceContext& deviceContext);
			void DrawLines			(crossplatform::DeviceContext& deviceContext,crossplatform::PosColourVertex *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false);
			void Draw2dLines		(crossplatform::DeviceContext& deviceContext,crossplatform::PosColourVertex *lines,int count,bool strip);
			void DrawCircle			(crossplatform::DeviceContext& context,const float *dir,float rads,const float *colr,bool fill=false);
			void ApplyDefaultMaterial();
			void SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *mat,const crossplatform::PhysicalLightRenderData &physicalLightRenderData);
			
            void ApplyCurrentPass(crossplatform::DeviceContext& deviceContext);

            crossplatform::Material*                CreateMaterial();
			crossplatform::Mesh*                    CreateMesh();
			crossplatform::Light*                   CreateLight();
			crossplatform::Texture*                 CreateTexture(const char *lFileNameUtf8= nullptr) override;
			crossplatform::BaseFramebuffer*         CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState*            CreateSamplerState(crossplatform::SamplerStateDesc *);
			crossplatform::Effect*                  CreateEffect();
			crossplatform::Effect*                  CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines);
			crossplatform::PlatformConstantBuffer*  CreatePlatformConstantBuffer();
			crossplatform::PlatformStructuredBuffer*CreatePlatformStructuredBuffer();
			crossplatform::Buffer*                  CreateBuffer();
			crossplatform::Layout*                  CreateLayout(int num_elements,const crossplatform::LayoutDesc *);
			crossplatform::RenderState*             CreateRenderState(const crossplatform::RenderStateDesc &desc);
			crossplatform::Query*                   CreateQuery(crossplatform::QueryType type) override;
			crossplatform::Shader*                  CreateShader() override;
			void*                                   GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext, int slot, int num_buffers,crossplatform::Buffer *const*buffers, const crossplatform::Layout *layout, const int *vertexSteps = NULL);
			
			void									SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0);
			void									ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth);
			void									DeactivateRenderTargets(crossplatform::DeviceContext &) override;
			void									SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps);

			void									SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer);
			
			void									SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t) override;
			void									EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options);

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									PopRenderTargets(crossplatform::DeviceContext &deviceContext);
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s);
			void									Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source);
			void									SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8);
			
            static GLenum                           toGLTopology(crossplatform::Topology t);
            static GLenum                           toGLMinFiltering(crossplatform::SamplerStateDesc::Filtering f);
            static GLenum                           toGLMaxFiltering(crossplatform::SamplerStateDesc::Filtering f);
            static GLenum                           toGLWrapping(crossplatform::SamplerStateDesc::Wrapping w);
			static                                  GLuint ToGLFormat(crossplatform::PixelFormat p);
			static                                  crossplatform::PixelFormat RenderPlatform::FromGLFormat(GLuint p);
			static                                  GLuint ToGLExternalFormat(crossplatform::PixelFormat p);
			static                                  GLenum DataType(crossplatform::PixelFormat p);
			static int                              FormatCount(crossplatform::PixelFormat p);
            
            void                                    MakeTextureResident(GLuint64 handle);

            GLuint                                  GetHelperFBO();
        private:
            GLuint              mNullVAO;
            GLenum              mCurTopology;
            std::set<GLuint64>  mResidentTextures;

            GLuint              mHelperFBO;
            GLint               mMaxViewports;
            GLint               mMaxColorAttatch;
		};
	}
}
