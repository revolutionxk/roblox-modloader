#include <doctest/doctest.h>

#include "RobloxModLoader/roblox/util/array_view.hpp"
#include "RobloxModLoader/roblox/reflection/creatable.hpp"

#include <type_traits>

TEST_CASE("ArrayView is two machine words passed by value")
{
	static_assert(sizeof(RBX::ArrayView<const int*>) == 16);
	static_assert(std::is_trivially_copyable_v<RBX::ArrayView<const int*>>);
	CHECK(true);
}

TEST_CASE("ICreator is a bare interface")
{
	static_assert(sizeof(RBX::ICreator) == sizeof(void*));
	static_assert(std::is_abstract_v<RBX::ICreator>);
	CHECK(true);
}
