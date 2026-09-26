#pragma once

#include "resource.hpp"
#include "types.hpp"

#include <memory>
#include <vector>

namespace RBX::Graphics
{
	class Texture : public Resource
	{
	public:
		enum class Type : std::uint32_t
		{
			Type_2D,
			Type_3D,
			Type_Cube,
			Type_2DArray
		};

		enum class Format : std::uint32_t
		{
			R8,
			R8UINT,
			RG8,
			RGB5A1,
			RGBA8,
			BGRA8,
			RGBA16UINT,
			RGBA32UINT,
			RG16,
			R16F,
			RG16F,
			RGBA16F,
			R32F,
			RG32F,
			RGBA32F,
			R16I,
			R32I,
			BC1,
			BC2,
			BC3,
			PVRTC_2BPP,
			PVRTC_4BPP,
			ETC1,
			ETC2_RGB,
			ETC2_RGBA,
			ASTC_4X4,
			ASTC_6X6,
			ASTC_8X8,
			D16,
			D24,
			D24S8,
			D32F,
			D32FS8,
			RGB10A2,
			RG11B10F,
			BC4,
			BC5,
			G8BR8_YUV_420,
			G8B8R8_YUV_420,
			RGBA16,
			Count
		};

		enum class Usage : std::uint32_t
		{
			Static = 0,
			ShaderRead = 1 << 0,
			RenderTarget = 1 << 1,
			AsyncDownload = 1 << 3,
			ShaderWrite = 1 << 5,
			Memoryless = 1 << 6
		};

		virtual void upload(unsigned index, unsigned mip, const TextureRegion& region, const void* data, unsigned size) = 0;
		virtual void download(unsigned index, unsigned mip, void* data, unsigned size) = 0;
		virtual std::shared_ptr<TextureDownloadBuffer> create_async_download_buffer(unsigned index, unsigned mip) = 0;
		virtual void async_download(const std::shared_ptr<TextureDownloadBuffer>& buffer) = 0;
		virtual bool supports_locking() const = 0;
		virtual void* lock(unsigned index, unsigned mip, const TextureRegion& region) = 0;
		virtual void unlock(unsigned index, unsigned mip) = 0;
		virtual void commit_changes() = 0;
		virtual bool supports_mip_level_reduction(unsigned levels) const = 0;
		virtual void reduce_mip_levels(unsigned levels) = 0;

		std::uint32_t type;
		std::uint32_t format;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t depth;
		std::uint32_t mip_levels;
		std::uint32_t array_length;
		std::uint32_t samples;
		std::uint32_t usage;
		std::uint32_t reserved_5c;
		void* reserved_60;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Texture, type, 0x40);
	RML_ASSERT_OFFSET(Texture, samples, 0x5c);
	RML_ASSERT_SIZE(Texture, 0x70);
#else
	RML_ASSERT_OFFSET(Texture, type, 0x38);
	RML_ASSERT_OFFSET(Texture, samples, 0x54);
	RML_ASSERT_SIZE(Texture, 0x68);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()

	struct Renderbuffer
	{
		std::shared_ptr<Texture> texture;
		std::uint64_t index;
		std::uint32_t mip;
		std::uint32_t reserved_1c;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(Renderbuffer, 0x20);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class Framebuffer : public Resource
	{
	public:
		union
		{
			std::vector<Renderbuffer> color;
		};
		union
		{
			Renderbuffer depth;
		};
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t samples;
		std::uint32_t mask;
		std::uint32_t size[2];
		std::uint32_t format;
		std::uint32_t reserved_8c;

	protected:
		Framebuffer()
		{
		}

	public:
		~Framebuffer() override
		{
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Framebuffer, color, 0x40);
	RML_ASSERT_OFFSET(Framebuffer, depth, 0x58);
	RML_ASSERT_OFFSET(Framebuffer, width, 0x78);
	RML_ASSERT_OFFSET(Framebuffer, size, 0x88);
	RML_ASSERT_OFFSET(Framebuffer, format, 0x90);
#else
	RML_ASSERT_OFFSET(Framebuffer, color, 0x38);
	RML_ASSERT_OFFSET(Framebuffer, depth, 0x50);
	RML_ASSERT_OFFSET(Framebuffer, width, 0x70);
	RML_ASSERT_OFFSET(Framebuffer, size, 0x80);
	RML_ASSERT_OFFSET(Framebuffer, format, 0x88);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
