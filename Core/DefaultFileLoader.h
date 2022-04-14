#pragma once
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/Export.h"
#if defined(__ANDROID__)
#include "android/asset_manager.h"
#endif
namespace platform
{
	namespace core
	{
		//! The default derived file loader
		class PLATFORM_CORE_EXPORT DefaultFileLoader:public platform::core::FileLoader
		{
		public:
			DefaultFileLoader();
			bool FileExists(const char *filename_utf8) const override;
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text) override;
			double GetFileDate(const char* filename_utf8) const override;
			void ReleaseFileContents(void* pointer) override;
			bool Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text) override;

		#if defined(__ANDROID__)
			static void SetAndroid_AAssetManager(AAssetManager* assetManager) { s_AssetManager = assetManager; };
			static AAssetManager* s_AssetManager;
		#endif
		};
	}
}