#pragma once

#include "RobloxModLoader/roblox/graphics/shader.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace FW
{
	class FileWatcher;
}

namespace RBX
{
	struct StringView
	{
		const char* data;
		std::size_t size;

		StringView() :
		    data(nullptr),
		    size(0)
		{
		}

		StringView(const char* text, const std::size_t length) :
		    data(text),
		    size(length)
		{
		}

		StringView(const std::string_view text) :
		    data(text.data()),
		    size(text.size())
		{
		}

		StringView(const char* text) :
		    StringView(std::string_view(text))
		{
		}

		std::string_view view() const
		{
			return {data, size};
		}
	};

	static_assert(std::is_trivially_copyable_v<StringView> && sizeof(StringView) == 16);
}

namespace RBX::Graphics
{
	class VisualEngine;

	class IShaderManager
	{
	public:
		virtual ~IShaderManager() = default;
		virtual void load_shaders(const std::string& pack, const std::string& vertex_suffix, const std::string& fragment_suffix, bool async) = 0;
		virtual void update_file_watcher() = 0;
		virtual void do_shader_manager_ui() = 0;
		virtual std::shared_ptr<ShaderProgram> get_program(StringView name, unsigned flags) = 0;
		virtual std::shared_ptr<ShaderProgram> get_program(StringView name) = 0;
		virtual std::shared_ptr<ShaderProgram> get_program(StringView vertex, unsigned vertex_flags, StringView fragment, unsigned fragment_flags) = 0;
		virtual std::shared_ptr<ShaderProgram> get_program(StringView vertex, StringView fragment) = 0;

	protected:
		IShaderManager() = default;
	};

	class ShaderManager : public IShaderManager
	{
	public:
		class DeferredShaderHandle;

		VisualEngine* visual_engine;
		union {
			std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
		};
		std::byte deferred_shaders[40];
		union {
			std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> programs;
		};
		union {
			std::vector<std::string> shader_names;
		};
		union {
			std::unordered_map<std::string, unsigned> shader_flags;
		};
		bool reserved_200;
		std::byte reserved_201[7];
		union {
			std::string pack_name;
		};
		std::uint64_t reserved_232;
		bool reserved_240;
		std::byte reserved_241[15];
		union {
			std::unique_ptr<FW::FileWatcher> file_watcher;
		};
		union {
			std::vector<std::string> watched_paths;
		};
		union {
			std::string key_scratch;
		};
		bool deferred_loading;
		std::byte reserved_313[7];

		~ShaderManager() override
		{
		}

		ShaderProgram* find_program(const std::string_view vertex, const std::string_view fragment) const
		{
			std::string key(vertex);
			key.push_back(',');
			key.append(fragment);
			key.push_back(',');
			for (const auto& [name, program] : programs)
				if (name == key)
					return program.get();
			return nullptr;
		}

	private:
		ShaderManager() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(ShaderManager, visual_engine, 8);
	RML_ASSERT_OFFSET(ShaderManager, shaders, 16);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(ShaderManager, deferred_shaders, 0x50);
	RML_ASSERT_OFFSET(ShaderManager, programs, 0x78);
	RML_ASSERT_OFFSET(ShaderManager, shader_names, 0xb8);
	RML_ASSERT_OFFSET(ShaderManager, shader_flags, 0xd0);
	RML_ASSERT_OFFSET(ShaderManager, pack_name, 0x118);
	RML_ASSERT_OFFSET(ShaderManager, file_watcher, 0x150);
	RML_ASSERT_OFFSET(ShaderManager, watched_paths, 0x158);
	RML_ASSERT_OFFSET(ShaderManager, key_scratch, 0x170);
	RML_ASSERT_OFFSET(ShaderManager, deferred_loading, 0x190);
	RML_ASSERT_SIZE(ShaderManager, 0x198);
#else
	RML_ASSERT_OFFSET(ShaderManager, deferred_shaders, 56);
	RML_ASSERT_OFFSET(ShaderManager, programs, 96);
	RML_ASSERT_OFFSET(ShaderManager, shader_names, 136);
	RML_ASSERT_OFFSET(ShaderManager, shader_flags, 160);
	RML_ASSERT_OFFSET(ShaderManager, pack_name, 208);
	RML_ASSERT_OFFSET(ShaderManager, file_watcher, 256);
	RML_ASSERT_OFFSET(ShaderManager, watched_paths, 264);
	RML_ASSERT_OFFSET(ShaderManager, key_scratch, 288);
	RML_ASSERT_OFFSET(ShaderManager, deferred_loading, 312);
	RML_ASSERT_SIZE(ShaderManager, 320);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
