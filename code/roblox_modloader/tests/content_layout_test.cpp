#include "RobloxModLoader/roblox/content_id.hpp"
#include "RobloxModLoader/roblox/content_provider_temporary_id_factory.hpp"
#include "RobloxModLoader/roblox/subsystem.hpp"

#include <doctest/doctest.h>

TEST_CASE("content mirrors keep the measured layout")
{
	static_assert(sizeof(RBX::ContentId) == 8 + sizeof(std::string));
#if defined(_WIN32)
	// MSVC's std::unordered_map is 64 bytes where libc++'s is 40, and each map here moves the
	// pair that follows it.
	static_assert(sizeof(RBX::ContentProviderTemporaryIdFactory) == 160);
	static_assert(offsetof(RBX::ContentProviderTemporaryIdFactory, buffers) == 96);
#else
	static_assert(sizeof(RBX::ContentProviderTemporaryIdFactory) == 112);
	static_assert(offsetof(RBX::ContentProviderTemporaryIdFactory, buffers) == 72);
#endif
	static_assert(offsetof(RBX::SubsystemHandle<int>, instance) == 24);
	CHECK(static_cast<int>(RBX::ContentIdType::Temporary) == 4);
	CHECK(static_cast<int>(RBX::ContentIdType::AssetId) == 5);
	CHECK(static_cast<int>(RBX::ContentIdType::Runtime) == 14);
}
