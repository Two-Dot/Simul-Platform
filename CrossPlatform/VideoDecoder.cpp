#include "VideoDecoder.h"
#include "Platform/Core/RuntimeError.h"

using namespace simul;
using namespace crossplatform;

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)          { if(p) { delete p; p=nullptr;} }
#endif

VideoDecoder::VideoDecoder()
	: mRenderPlatform(nullptr)
	, mInputBuffer(nullptr)
	, mDecodeFence(nullptr)
	, mFeaturesSupported(false)
{

}

VideoDecoder::~VideoDecoder()
{

}

VideoDecoderResult VideoDecoder::Initialize(simul::crossplatform::RenderPlatform* renderPlatform, const VideoDecoderParams& decoderParams)
{
	mRenderPlatform = renderPlatform;
	mDecoderParams = decoderParams;
	mFeaturesSupported = false;

	VideoDecoderResult result = Init();
	if (DEC_FAILED(result))
	{
		Shutdown();
		return result;
	}

	mInputBuffer = CreateVideoBuffer();
	mInputBuffer->EnsureBuffer(mRenderPlatform, VideoBufferType::DECODE_READ, 250000);

	mTextures.resize(decoderParams.maxDecodePictureBufferCount);

	for (int i = 0; i < mTextures.size(); ++i)
	{
		mTextures[i] = CreateDecoderTexture();
		mTextures[i]->ensureVideoTexture(mRenderPlatform, mDecoderParams.width, mDecoderParams.height, mDecoderParams.decodeFormat, VideoTextureType::DECODE);
	}

	mDecodeFence = mRenderPlatform->CreateFence("DecodeFence");

	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::Decode(Texture* outputTexture, const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs, uint32_t decodeArgCount)
{
	DecodeFrame(outputTexture, buffer, bufferSize, decodeArgs, decodeArgCount);
	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::Shutdown()
{
	SAFE_DELETE(mDecodeFence);
	for (auto texture : mTextures)
	{
		SAFE_DELETE(texture);
	}
	mTextures.clear();
	SAFE_DELETE(mInputBuffer);
	mDecoderParams = {};
	mRenderPlatform = nullptr;
	return VideoDecoderResult::Ok;
}

bool VideoDecoder::IsIDR(const uint8_t* data, size_t size) const
{
	assert(size > 0);
	if (mDecoderParams.codec == VideoCodec::H264)
	{
		if ((data[0] & 31) == 5)
		{
			return true;
		}
	}
	else if (mDecoderParams.codec == VideoCodec::HEVC)
	{
		uint8_t type = (data[0] & 0x7e) >> 1;

		// IDR_W_DLP or IDR_N_LP  
		if (type == 19 || type == 20)
		{
			return true;
		}
	}
	
	return false;
}
