#pragma once

#include <vulkan/vulkan.hpp>
#include "Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/PixelFormat.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
	#pragma warning(disable:4275)
#endif
#define MAX_STACKED_TARGETS 10

#ifdef UNIX
template <typename T, std::size_t N>
constexpr std::size_t _countof(T const (&)[N]) noexcept
{
return N;
}
#endif

namespace vk
{
	class Instance;
}

namespace platform
{
	namespace vulkan
	{
		extern bool debugUtilsSupported;
		extern bool debugMarkerSupported;
		template<typename T>
		void SetVulkanName(crossplatform::RenderPlatform* renderPlatform, const T& ds, const char* name)
		{
			vk::Instance* instance = renderPlatform->AsVulkanInstance();
			vk::Device* device = renderPlatform->AsVulkanDevice();
			uint64_t objectHandle = *((uint64_t*)&ds);

			if(debugUtilsSupported)
			{
				vk::DispatchLoaderDynamic d;
				d.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)instance->getProcAddr("vkSetDebugUtilsObjectNameEXT");
				if (d.vkSetDebugUtilsObjectNameEXT)
				{
					vk::DebugUtilsObjectNameInfoEXT nameInfo;
					nameInfo
						.setPNext(nullptr)
						.setObjectType(ds.objectType)
						.setObjectHandle(objectHandle)
						.setPObjectName(name);

					device->setDebugUtilsObjectNameEXT(nameInfo, d);
				}
			}
			if(debugMarkerSupported)
			{
				vk::DispatchLoaderDynamic d;
				d.vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)instance->getProcAddr("vkDebugMarkerSetObjectNameEXT");
				if (d.vkDebugMarkerSetObjectNameEXT)
				{
					vk::DebugMarkerObjectNameInfoEXT nameInfo;
					nameInfo
						.setPNext(nullptr)
						.setObjectType(ds.debugReportObjectType)
						.setObject(objectHandle)
						.setPObjectName(name);

					device->debugMarkerSetObjectNameEXT(nameInfo, d);
				}
			}

			if (platform::core::SimulInternalChecks)
			{
				crossplatform::RenderPlatform::ResourceMap[objectHandle] = name;
		#ifdef _DEBUG
				std::cout << "0x" << std::hex << objectHandle << "\t" << name << "\n";
		#endif
			}
		}
		template<typename T>
		void SetVulkanName(crossplatform::RenderPlatform* renderPlatform, const T& ds, const std::string& name)
		{
			SetVulkanName(renderPlatform, ds, name.c_str());
		}

		class Material;
		class Texture;

		//! Vulkan renderplatform implementation
		class SIMUL_VULKAN_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
						RenderPlatform();
			virtual		~RenderPlatform() override;

			vk::Device *AsVulkanDevice() override;
			vk::Instance *AsVulkanInstance() override;
			vk::PhysicalDevice *GetVulkanGPU();
			uint32_t GetInstanceAPIVersion();
			uint32_t GetPhysicalDeviceAPIVersion();
			bool CheckInstanceExtension(const std::string& instanceExtensionName);
			bool CheckDeviceExtension(const std::string& deviceExtensionName);

			template<typename T>
			void FillPhysicalDeviceFeatures2ExtensionStructure(T& _structure)
			{
				vk::PhysicalDeviceFeatures2 features2;
				features2.pNext = &_structure;

				//Execute calls into VK_KHR_get_physical_device_properties2
				if (GetInstanceAPIVersion() >= VK_API_VERSION_1_1)
				{
					vulkanGpu->getFeatures2(&features2);
				}
				else if (CheckDeviceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
				{
					vk::DispatchLoaderDynamic d;
					d.vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)vulkanInstance->getProcAddr("vkGetPhysicalDeviceFeatures2");
					vulkanGpu->getFeatures2(&features2, d);
				}
			}

			template<typename T>
			void FillPhysicalDeviceProperties2ExtensionStructure(T& _structure)
			{
				vk::PhysicalDeviceProperties2 properties2;
				properties2.pNext = &_structure;

				//Execute calls into VK_KHR_get_physical_device_properties2
				if (GetInstanceAPIVersion() >= VK_API_VERSION_1_1)
				{
					vulkanGpu->getProperties2(&properties2);
				}
				else if (CheckDeviceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
				{
					vk::DispatchLoaderDynamic d;
					d.vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)vulkanInstance->getProcAddr("vkGetPhysicalDeviceProperties2");
					vulkanGpu->getProperties2(&properties2, d);
				}
			}

			void ExecuteCommands(crossplatform::DeviceContext& deviceContext) override;
			void RestartCommands(crossplatform::DeviceContext& deviceContext) override;

			void PushToReleaseManager(vk::Buffer &);
			void PushToReleaseManager(vk::Pipeline& r);
			void PushToReleaseManager(vk::PipelineCache& r);
			void PushToReleaseManager(vk::BufferView &);
			void PushToReleaseManager(vk::DeviceMemory &);
			void PushToReleaseManager(vk::ImageView&);
			void PushToReleaseManager(vk::Framebuffer&);
			void PushToReleaseManager(vk::RenderPass& r);
			void PushToReleaseManager(vk::Image& i);
			void PushToReleaseManager(vk::Sampler& i);
			void PushToReleaseManager(vk::PipelineLayout& i);
			void PushToReleaseManager(vk::DescriptorSetLayout& i);
			void PushToReleaseManager(vk::DescriptorPool& i); //All descriptor sets allocated from the pool are implicitly freed and become invalid.
			void ClearReleaseManager();
			const char* GetName() const override;
			crossplatform::RenderPlatformType GetType() const override
			{
				return crossplatform::RenderPlatformType::Vulkan;
			}
			virtual const char *GetSfxConfigFilename() const override
			{
				return "Sfx/Vulkan.json";
			}
			void									RestoreDeviceObjects(void*) override;
			void									InvalidateDeviceObjects() override;
			void									RecompileShaders() override;
			void									BeginFrame() override;
			void									EndFrame() override;
			void									CopyTexture(crossplatform::DeviceContext& deviceContext, crossplatform::Texture *, crossplatform::Texture *) override;
			float									GetDefaultOutputGamma() const override;
			void									BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)override;
			void									EndEvent(crossplatform::DeviceContext& deviceContext)override;
			void									DispatchCompute(crossplatform::DeviceContext& deviceContext, int w, int l, int d) override;
			
			void									ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex) override;
			void									ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb) override;
			
			void									Draw(crossplatform::GraphicsDeviceContext& deviceContext, int num_verts, int start_vert) override;
			void									DrawIndexed(crossplatform::GraphicsDeviceContext& deviceContext, int num_indices, int start_index = 0, int base_vertex = 0) override;
			void									DrawQuad(crossplatform::GraphicsDeviceContext& deviceContext) override;
			void									GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx = -1)override;
			//! This should be called after a Draw/Dispatch command that uses
			//! textures. Here we will apply the textures.
			bool									ApplyContextState(crossplatform::DeviceContext &deviceContext,bool error_checking=true) override;
			
			void									InsertFences(crossplatform::DeviceContext& deviceContext);

			crossplatform::Material*				CreateMaterial();
			crossplatform::Framebuffer*			CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState*			CreateSamplerState(crossplatform::SamplerStateDesc *) override;
			crossplatform::Effect*					CreateEffect() override;
			crossplatform::PlatformConstantBuffer*	CreatePlatformConstantBuffer() override;
			crossplatform::PlatformStructuredBuffer*CreatePlatformStructuredBuffer() override;
			crossplatform::Buffer*					CreateBuffer() override;
			crossplatform::RenderState*				CreateRenderState(const crossplatform::RenderStateDesc &desc) override;
			crossplatform::Query*					CreateQuery(crossplatform::QueryType type) override;
			crossplatform::Shader*					CreateShader() override;

			crossplatform::DisplaySurface*			CreateDisplaySurface() override;
			void*									GetDevice();
			
			void									SetStreamOutTarget(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0) override;
			void									ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth) override;
			void									DeactivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext) override;

			void									SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps) override;

			void									EnsureEffectIsBuilt(const char *filename_utf8) override;

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext) override;
			void									PopRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
			void									SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)override;
			void									Resolve(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::GraphicsDeviceContext&,crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			void									RestoreColourTextureState(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex) override;
			void									RestoreDepthTextureState(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex) override;
			
			static vk::PrimitiveTopology			toVulkanTopology(crossplatform::Topology t);
			static vk::CullModeFlags				toVulkanCullFace(crossplatform::CullFaceMode c);
			static vk::PolygonMode					toVulkanPolygonMode(crossplatform::PolygonMode p);
			static vk::CompareOp					toVulkanComparison(crossplatform::DepthComparison d);
			static vk::BlendFactor					toVulkanBlendFactor(crossplatform::BlendOption o);
			static vk::BlendOp						toVulkanBlendOperation(crossplatform::BlendOperation o);
			static vk::Filter						toVulkanMinFiltering(crossplatform::SamplerStateDesc::Filtering f);
			static vk::Filter						toVulkanMaxFiltering(crossplatform::SamplerStateDesc::Filtering f);
			static vk::SamplerMipmapMode			toVulkanMipmapMode(crossplatform::SamplerStateDesc::Filtering f);
			static vk::SamplerAddressMode			toVulkanWrapping(crossplatform::SamplerStateDesc::Wrapping w);
			static vk::Format						ToVulkanFormat(crossplatform::PixelFormat p,crossplatform::CompressionFormat c=crossplatform::CompressionFormat::UNCOMPRESSED);
			static crossplatform::PixelFormat		FromVulkanFormat(vk::Format p);
			static vk::Format						ToVulkanExternalFormat(crossplatform::PixelFormat p);
			static vk::ImageLayout					ToVulkanImageLayout(crossplatform::ResourceState r);
			//static Vulkanenum						DataType(crossplatform::PixelFormat p);
			static int								FormatTexelBytes(crossplatform::PixelFormat p);
			static int								FormatCount(crossplatform::PixelFormat p);
			bool									MemoryTypeFromProperties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex);
			
			static bool								memory_type_from_properties(vk::PhysicalDevice *gpu,uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex);
			
			//! Makes the handle resident only if its not resident already
			vulkan::Texture*						GetDummyTextureCube();
			//! Returns 2DMS dummy texture 1 white texel at 2x MS
			vulkan::Texture*						GetDummy2DMS();
			//! Returns 2D Array dummy texture 1 white texel
			vulkan::Texture*						GetDummy2DArray();
			//! Returns 2D dummy texture 1 white texel
			vulkan::Texture*						GetDummy2D();
			//! Returns 3D dummy texture 1 white texel
			vulkan::Texture*						GetDummy3D();
			//! Returns dummy texture chosen from resource type.
			vulkan::Texture*						GetDummyTexture(crossplatform::ShaderResourceType);
			
			vk::Framebuffer*						GetCurrentVulkanFramebuffer(crossplatform::GraphicsDeviceContext& deviceContext);
			static crossplatform::PixelFormat		GetActivePixelFormat(crossplatform::GraphicsDeviceContext &deviceContext);
			static int								GetActiveNumOfSamples(crossplatform::GraphicsDeviceContext &deviceContext);
			static vk::Extent2D						GetTextureTargetExtext2D(const crossplatform::TargetsAndViewport::TextureTarget& targetTexture);
			static vk::Extent2D						GetTargetAndViewportExtext2D(const crossplatform::TargetsAndViewport* targetsAndViewport);
			
			uint32_t								FindMemoryType(uint32_t typeFilter,vk::MemoryPropertyFlags properties);
			void									CreateVulkanBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory,const char *name);			
			void									CreateVulkanRenderpass(crossplatform::DeviceContext& deviceContext, vk::RenderPass &renderPass
																			,int num_colour,const crossplatform::PixelFormat *pixelFormats
																			,crossplatform::PixelFormat depthFormat=crossplatform::PixelFormat::UNKNOWN
																			,bool depthTest=false,bool depthWrite=false
																			,bool clear=false
																			,int numOfSamples=1
																			,bool multiview=false
																			,const vk::ImageLayout *initial_layouts=nullptr,const vk::ImageLayout *final_layouts=nullptr);
			static void								SetDefaultColourFormat(crossplatform::PixelFormat p);
			virtual void							InvalidCachedFramebuffersAndRenderPasses() override;
			void									EndRenderPass(crossplatform::DeviceContext& deviceContext) override;
			static std::string						VulkanResultString(vk::Result res);

			// Vulkan-specific support for video decoding:
			vk::Sampler								GetSamplerYcbcr() { return vulkanSamplerYcbcr; }
			vk::SamplerYcbcrConversionInfo*			GetSamplerYcbcrConversionInfo();
		protected:
			vk::SamplerYcbcrConversionInfo samplerYcbcrConversionInfo;

		protected:
			crossplatform::Texture*					createTexture() override;
			vk::Instance*									vulkanInstance=nullptr;
			vk::PhysicalDevice*								vulkanGpu=nullptr;
			vk::Device*										vulkanDevice=nullptr;
			vk::Sampler										vulkanSamplerYcbcr;

			bool											resourcesToBeReleased=false;
			std::set<vk::Buffer>							releaseBuffers;
			std::set<vk::BufferView>						releaseBufferViews;
			std::set<vk::DeviceMemory>						releaseMemories;
			std::set<vk::ImageView>							releaseImageViews;
			std::set<vk::Framebuffer>						releaseFramebuffers;
			std::set<vk::RenderPass>						releaseRenderPasses;
			std::set<vk::Image>								releaseImages;
			std::set<vk::Sampler>							releaseSamplers;
			std::set<vk::Pipeline>							releasePipelines;
			std::set<vk::PipelineCache>						releasePipelineCaches;
			std::set<vk::PipelineLayout>					releasePipelineLayouts;
			std::set<vk::DescriptorSetLayout>				releaseDescriptorSetLayouts;
			std::set<vk::DescriptorPool>					releaseDescriptorPools;

			vulkan::Texture*								mDummy2D=nullptr;
			vulkan::Texture*								mDummy2DArray = nullptr;
			vulkan::Texture*								mDummy2DMS=nullptr;
			vulkan::Texture* mDummy3D = nullptr;
			vulkan::Texture*								mDummyTextureCube=nullptr;
			vulkan::Texture*								mDummyTextureCubeArray=nullptr;
			vk::DescriptorPool								mDescriptorPool;
			static crossplatform::PixelFormat				defaultColourFormat;
			unsigned long long InitFramebuffer(crossplatform::DeviceContext& deviceContext,crossplatform::TargetsAndViewport *tv);
			std::map<unsigned long long,vk::Framebuffer>	mFramebuffers;
			std::map<unsigned long long,crossplatform::TargetsAndViewport>				mTargets;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
