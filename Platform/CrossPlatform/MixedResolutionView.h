#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/mixed_resolution_constants.sl"
#include "Simul/Geometry/Orientation.h"
#include <set>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class BaseFramebuffer;
		class TwoResFramebuffer;
		class Effect;
		struct Viewport;
		///
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		class RenderPlatform;
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of MixedResolutionView will be created and maintained for each live 3D view.
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionView
		{
		public:
			/// Default constructor.
			MixedResolutionView();
			/// Destructor.
			~MixedResolutionView();

			/// Restore device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void InvalidateDeviceObjects();

			/// Gets screen width.
			///
			/// \return	The screen width.
			int GetScreenWidth() const;

			/// Gets screen height.
			///
			/// \return	The screen height.
			int GetScreenHeight() const;

			/// Sets the resolution.
			///
			/// \param	w	The width.
			/// \param	h	The height.
			void SetResolution(int w,int h);

			/// Sets external framebuffer.
			///
			/// \param	ext	Whether to use an external framebuffer.
			void SetExternalFramebuffer(bool ext);
			/// Gets resolved header buffer.
			///
			/// \return	null if it fails, else the resolved header buffer.
			crossplatform::Texture						*GetResolvedHDRBuffer();

			/// Gets the framebuffer.
			///
			/// \return	null if it fails, else the framebuffer.
			crossplatform::BaseFramebuffer				*GetFramebuffer()
			{
				return hdrFramebuffer;
			}
			/// Type of view.
			ViewType									viewType;
			bool										vrDistortion;
		private:
			///      A framebuffer with depth.
			simul::crossplatform::BaseFramebuffer		*hdrFramebuffer;
			/// The resolved texture.
			simul::crossplatform::Texture				*resolvedTexture;
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
		public:
			/// Width of the screen.
			int								ScreenWidth;
			/// Height of the screen.
			int								ScreenHeight;
		public:
			/// true to use external framebuffer.
			bool							useExternalFramebuffer;
		};
		/// A class to render mixed-resolution depth buffers.
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionRenderer
		{
		public:
			/// Default constructor.
			MixedResolutionRenderer();
			/// Destructor.
			~MixedResolutionRenderer();

			/// Restore device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void InvalidateDeviceObjects();
			/// Recompile shaders.
			void RecompileShaders();

			void DownscaleDepth(crossplatform::DeviceContext &deviceContext
				,crossplatform::Texture *depthTexture
				,const crossplatform::Viewport *simulViewport
				,crossplatform::TwoResFramebuffer *fb
				,int downscale,float max_dist_metres);
		protected:
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
			/// The depth forward effect.
			crossplatform::Effect						*depthForwardEffect;
			/// The depth reverse effect.
			crossplatform::Effect						*depthReverseEffect;
			/// The constant buffer for the shader effect.
			crossplatform::ConstantBuffer<MixedResolutionConstants>	mixedResolutionConstants;
		};

		/// A class to store a set of MixedResolutionView objects, one per view id.
		class SIMUL_CROSSPLATFORM_EXPORT MixedResolutionViewManager
		{
		public:
			/// Default constructor.
			MixedResolutionViewManager():
				last_created_view_id(-1)
				,renderPlatform(NULL)
			{}

			/// Restore the device objects.
			///
			/// \param [in,out]	renderPlatform	If non-null, the render platform.
			void							RestoreDeviceObjects	(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void							InvalidateDeviceObjects	();

			/// Gets a view.
			///
			/// \param	view_id	Identifier for the view.
			///
			/// \return	null if it fails, else the view.
			MixedResolutionView				*GetView				(int view_id);

			/// Gets the views.
			///
			/// \return	null if it fails, else the views.
			std::set<MixedResolutionView*>	GetViews				();

			/// Adds a view.
			///
			/// \param	external_framebuffer	Whether to use an external framebuffer.
			///
			/// \return	An int view_id.
			int								AddView					(bool external_framebuffer);

			/// Removes the view.
			///
			/// \param	view_id	The id of the view to remove.
			void							RemoveView				(int view_id);
			/// Clears this object to its blank/initial state.
			void							Clear					();
			/// The mixed resolution renderer.
			MixedResolutionRenderer			mixedResolutionRenderer;
		protected:
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
			typedef std::map<int,MixedResolutionView*>	ViewMap;
			ViewMap							views;
			/// Identifier for the last created view.
			int								last_created_view_id;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif