#pragma once

#include "resource.hpp"

#include <cstdint>

namespace RBX::Graphics
{
	class TextureDownloadBuffer : public Resource
	{
	public:
		struct View
		{
			const void* data;
			std::uint32_t stride;
			std::uint32_t size;
		};

		virtual bool copy_data(void* destination) = 0;
		virtual View data() const = 0;

		std::uint32_t index;
		std::uint32_t mip;
		std::uint32_t size;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(TextureDownloadBuffer::View, 16);
	RML_ASSERT_OFFSET(TextureDownloadBuffer::View, stride, 8);
	RML_ASSERT_OFFSET(TextureDownloadBuffer::View, size, 12);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(TextureDownloadBuffer, index, 0x40);
	RML_ASSERT_OFFSET(TextureDownloadBuffer, mip, 0x44);
	RML_ASSERT_OFFSET(TextureDownloadBuffer, size, 0x48);
	RML_ASSERT_SIZE(TextureDownloadBuffer, 0x50);
#else
	RML_ASSERT_OFFSET(TextureDownloadBuffer, index, 0x38);
	RML_ASSERT_OFFSET(TextureDownloadBuffer, mip, 0x3C);
	RML_ASSERT_OFFSET(TextureDownloadBuffer, size, 0x40);
	RML_ASSERT_SIZE(TextureDownloadBuffer, 0x48);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
