#pragma once

#include "RobloxModLoader/roblox/content_id.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace RBX
{
	class ReadOnlySharedBuffer;
}

template<>
struct std::hash<RBX::ContentId>
{
	std::size_t operator()(const RBX::ContentId& id) const noexcept
	{
		return std::hash<std::string>{}(id.id);
	}
};

namespace RBX
{
	class ContentProviderTemporaryIdFactory
	{
	public:
		const char* name;
		std::size_t name_length;
		std::byte files_mutex[8];
		union {
			std::unordered_map<ContentId, std::string> files;
		};
		std::byte buffers_mutex[8];
		union {
			std::unordered_map<ContentId, std::weak_ptr<const ReadOnlySharedBuffer>> buffers;
		};

		bool knows_temporary_id(const ContentId& id) const
		{
			for (const auto& [key, path] : files)
				if (key == id)
					return true;
			for (const auto& [key, buffer] : buffers)
				if (key == id)
					return true;
			return false;
		}

		const std::string* file_for_temporary_id(const ContentId& id) const
		{
			for (const auto& [key, path] : files)
				if (key == id)
					return &path;
			return nullptr;
		}

	private:
		ContentProviderTemporaryIdFactory() = delete;
		~ContentProviderTemporaryIdFactory() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, files_mutex, 16);
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, files, 24);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, buffers_mutex, 0x58);
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, buffers, 0x60);
	RML_ASSERT_SIZE(ContentProviderTemporaryIdFactory, 0xa0);
#else
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, buffers_mutex, 64);
	RML_ASSERT_OFFSET(ContentProviderTemporaryIdFactory, buffers, 72);
	RML_ASSERT_SIZE(ContentProviderTemporaryIdFactory, 112);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
