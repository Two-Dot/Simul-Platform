#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include <vector>
#include "Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
struct ID3D11Device;
struct VertexXyzRgba
{
	float x,y,z;
	float r,g,b,a;
};
namespace simul
{
	namespace crossplatform
	{
		class Material;
		class Effect;
		class EffectTechnique;
		struct EffectDefineOptions;
		class Light;
		class Texture;
		class Mesh;
		class PlatformConstantBuffer;
		class Buffer;
		class Layout;
		struct DeviceContext;
		struct LayoutDesc;
		/// A base class for API-specific rendering.

		/*! RenderPlatform is an interface that allows Simul's rendering functions to be developed
			in a cross-platform manner. By abstracting the common functionality of the different graphics API's
			into an interface, we can write render code that need not know which API is being used. It is possible
			to create platform-specific objects like /link CreateTexture textures/endlink, /link CreateEffect effects/endlink
			and /link CreateBuffer buffers/endlink

			Be sure to make the following calls at the appropriate places:
			RestoreDeviceObjects(), InvalidateDeviceObjects(), RecompileShaders(), SetReverseDepth()
			*/
		class SIMUL_CROSSPLATFORM_EXPORT RenderPlatform
		{
		public:
			struct Vertext
			{
				vec3 pos;
				vec4 colour;
			};
			virtual ID3D11Device *AsD3D11Device()=0;
			virtual void PushTexturePath	(const char *pathUtf8)=0;
			virtual void PopTexturePath		()=0;
			virtual void StartRender		()=0;
			virtual void EndRender			()=0;
			virtual void SetReverseDepth	(bool)=0;
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3])		=0;
			virtual void DispatchCompute	(DeviceContext &deviceContext,int w,int l,int d)=0;
			virtual void ApplyShaderPass	(DeviceContext &deviceContext,Effect *,EffectTechnique *,int)=0;
			virtual void Draw				(DeviceContext &deviceContext,int num_verts,int start_vert)=0;
			virtual void DrawMarker			(DeviceContext &deviceContext,const double *matrix)			=0;
			virtual void DrawLine			(DeviceContext &deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)=0;
			virtual void DrawCrossHair		(DeviceContext &deviceContext,const double *pGlobalPosition)	=0;
			virtual void DrawCamera			(DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)=0;
			virtual void DrawLineLoop		(DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4])=0;

			virtual void DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f)=0;
			virtual void DrawDepth			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex)=0;
			// Draw an onscreen quad without passing vertex positions, but using the "rect" constant from the shader to pass the position and extent of the quad.
			virtual void DrawQuad			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)=0;
			virtual void DrawQuad			(DeviceContext &deviceContext)=0;

			virtual void Print				(DeviceContext &deviceContext,int x,int y,const char *text)							=0;
			virtual void DrawLines			(DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false,bool test_depth=false)		=0;
			virtual void Draw2dLines		(DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)		=0;
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false)		=0;
			virtual void PrintAt3dPos		(DeviceContext &deviceContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0)		=0;
			virtual void					SetModelMatrix					(crossplatform::DeviceContext &deviceContext,const double *mat)	=0;
			virtual void					ApplyDefaultMaterial			()	=0;
			/// Create a platform-specific material instance.
			virtual Material				*CreateMaterial					()	=0;
			/// Create a platform-specific mesh instance.
			virtual Mesh					*CreateMesh						()	=0;
			/// Create a platform-specific light instance.
			virtual Light					*CreateLight					()	=0;
			/// Create a platform-specific texture instance.
			virtual Texture					*CreateTexture					(const char *lFileNameUtf8=NULL)	=0;
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					(const char *filename_utf8,const std::map<std::string,std::string> &defines)=0;
			/// Create a platform-specific constant buffer instance. This is not usually used directly, instead, create a
			/// simul::crossplatform::ConstantBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;
			/// Create a platform-specific buffer instance - e.g. vertex buffers, index buffers etc.
			virtual Buffer					*CreateBuffer					()	=0;
			/// Create a platform-specific layout instance based on the given layout description \em layoutDesc and buffer \em buffer.
			virtual Layout					*CreateLayout					(int num_elements,LayoutDesc *layoutDesc,Buffer *buffer)	=0;
			/// Activate the specifided vertex buffers in preparation for rendering.
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,Buffer **buffers)=0;
			/// This function is called to ensure that the named shader is compiled with all the possible combinations of \#define's given in \em options.
			void							EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<EffectDefineOptions> &options);
			/// Called to store the render state - blending, depth check, etc. - for later retrieval with RestoreRenderState.
			/// Some platforms may not support this.
			virtual void					StoreRenderState(crossplatform::DeviceContext &deviceContext)=0;
			/// Called to restore the render state previously stored with StoreRenderState. There must be exactly one call of RestoreRenderState
			/// for each StoreRenderState call, and they are not expected to be nested.
			virtual void					RestoreRenderState(crossplatform::DeviceContext &deviceContext)=0;
		private:
			void							EnsureEffectIsBuiltPartialSpec	(const char *filename_utf8,const std::vector<EffectDefineOptions> &options,const std::map<std::string,std::string> &defines);
		};

		/// Draw a horizontal grid, centred around the camera, at z=0.
		///
		/// \param [in,out]	deviceContext	Context for the device.
		extern SIMUL_CROSSPLATFORM_EXPORT void DrawGrid(crossplatform::DeviceContext &deviceContext);
	}
}
#endif