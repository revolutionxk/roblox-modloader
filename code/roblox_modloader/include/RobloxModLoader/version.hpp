#pragma once

#define RML_ABI_VERSION 7

namespace rml::version
{
	[[nodiscard]] const char* commit();
	[[nodiscard]] const char* commit_full();
	[[nodiscard]] const char* branch();
	[[nodiscard]] const char* date();
	[[nodiscard]] const char* subject();
	[[nodiscard]] bool dirty();
	[[nodiscard]] const char* string();
}
