#pragma once

#include "RobloxModLoader/internal/platform.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>

namespace RBX::Graphics
{
	class Device;
	class IShaderManager;
	class SceneManager;
	class SoftwareOcclusion;

	struct SoftwareOcclusionDeleter
	{
		void operator()(SoftwareOcclusion* occlusion) const;
	};

	class VisualEngine
	{
	private:
#if defined(RML_WINDOWS)
		[[maybe_unused]] std::byte reserved_0[0xD8];
#else
		[[maybe_unused]] std::byte reserved_0[0x108];
#endif

	public:
		Device* device;

	private:
		[[maybe_unused]] std::byte reserved_after_device[0xAB8];

	public:
		union {
			std::unique_ptr<IShaderManager> shader_manager;
		};

	private:
		union {
			[[maybe_unused]] std::shared_ptr<SoftwareOcclusion> reserved_software_occlusion_shared;
		};

	public:
		IShaderManager* external_shader_manager;

	private:
		union {
			[[maybe_unused]] std::unique_ptr<SoftwareOcclusion, SoftwareOcclusionDeleter> reserved_software_occlusion;
		};

	public:
		union {
			std::unique_ptr<SceneManager> scene_manager;
		};

		IShaderManager* get_shader_manager() const
		{
			return external_shader_manager ? external_shader_manager : shader_manager.get();
		}

		SceneManager* get_scene_manager() const
		{
			return scene_manager.get();
		}

	private:
		VisualEngine() = delete;
		~VisualEngine() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(VisualEngine, device, 0xD8);
	RML_ASSERT_OFFSET(VisualEngine, shader_manager, 0xB98);
	RML_ASSERT_OFFSET(VisualEngine, external_shader_manager, 0xBB0);
	RML_ASSERT_OFFSET(VisualEngine, scene_manager, 0xBC0);
#else
	RML_ASSERT_OFFSET(VisualEngine, device, 0x108);
	RML_ASSERT_OFFSET(VisualEngine, shader_manager, 0xBC8);
	RML_ASSERT_OFFSET(VisualEngine, external_shader_manager, 0xBE0);
	RML_ASSERT_OFFSET(VisualEngine, scene_manager, 0xBF0);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
