#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/Topology.h"

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
		class Effect;
		class BaseFramebuffer;
		struct Viewport;
		class RenderPlatform;
		///
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of View will be created and maintained for each live 3D view.
		class SIMUL_CROSSPLATFORM_EXPORT View
		{
		public:
			/// Default constructor.
			View();
			/// Destructor.
			virtual ~View();

			/// Restore device objects.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void InvalidateDeviceObjects();

			/// Gets screen width.
			int GetScreenWidth() const;

			/// Gets screen height.
			int GetScreenHeight() const;

			/// Sets the resolution.
			void SetResolution(int w,int h);

			/// Sets external framebuffer.
			void SetExternalFramebuffer(bool ext);
			/// Gets resolved HDR buffer.
			crossplatform::Texture						*GetResolvedHDRBuffer();

			/// Gets the framebuffer.
			crossplatform::BaseFramebuffer				*GetFramebuffer()
			{
				return hdrFramebuffer;
			}
			const crossplatform::BaseFramebuffer		*GetFramebuffer() const
			{
				return hdrFramebuffer;
			}
			/// Type of view.
			crossplatform::ViewType						viewType;
			bool										vrDistortion;
		private:
			/// A framebuffer with depth.
			simul::crossplatform::BaseFramebuffer		*hdrFramebuffer;
			/// The resolved texture.
			simul::crossplatform::Texture				*resolvedTexture;
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
		public:
			/// Width of the screen.
			int											ScreenWidth;
			/// Height of the screen.
			int											ScreenHeight;
			int											last_framenumber;
		public:
			/// true to use external framebuffer.
			bool										useExternalFramebuffer;
		};

		/// A class to store a set of View objects, one per view id.
		class SIMUL_CROSSPLATFORM_EXPORT ViewManager
		{
		public:
			/// Default constructor.
			ViewManager():
				renderPlatform(NULL)
				,last_created_view_id(-1)
			{}
			typedef std::map<int,View*>	ViewMap;
			/// Restore the device objects.
			void							RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			/// Invalidate device objects.
			void							InvalidateDeviceObjects	();
			/// Gets a view.
			/// \return	null if it fails, else the view.
			View			*GetView		(int view_id);
			const View		*GetView		(int view_id) const;
			/// Delete old views
			void CleanUp(int current_framenumber,int max_age);
			/// Gets the views.
			const ViewMap &GetViews() const;
			/// Adds a view.
			/// \return	An int view_id.
			int								AddView	();
			int								AddView	(View *v);
			/// Removes the view.
			void							RemoveView				(int view_id);
			/// Clears this object to its blank/initial state.
			void							Clear					();
		protected:
			/// The render platform.
			crossplatform::RenderPlatform				*renderPlatform;
			ViewMap							views;
			/// Identifier for the last created view.
			int								last_created_view_id;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif