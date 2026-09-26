#pragma once

#include "resource.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace RBX::Graphics
{
	class Shader : public Resource
	{
	public:
		enum class Type : std::uint32_t
		{
			Vertex,
			Fragment,
			Compute
		};

		virtual void reload(const std::vector<char>& bytecode) = 0;

		std::uint32_t type;
		std::uint32_t reserved_3c;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Shader, type, 0x40);
	RML_ASSERT_SIZE(Shader, 0x48);
#else
	RML_ASSERT_OFFSET(Shader, type, 0x38);
	RML_ASSERT_SIZE(Shader, 0x40);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()

	class ShaderProgram : public Resource
	{
	public:
		virtual void reload() = 0;

		std::shared_ptr<Shader> shaders[3];
		std::uint32_t buffer_mask;
		std::uint32_t texture_mask;

		const std::shared_ptr<Shader>& get_shader(const Shader::Type type) const
		{
			return shaders[static_cast<std::uint32_t>(type)];
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(ShaderProgram, shaders, 0x40);
	RML_ASSERT_OFFSET(ShaderProgram, buffer_mask, 0x70);
	RML_ASSERT_SIZE(ShaderProgram, 0x78);
#else
	RML_ASSERT_OFFSET(ShaderProgram, shaders, 56);
	RML_ASSERT_OFFSET(ShaderProgram, buffer_mask, 104);
	RML_ASSERT_SIZE(ShaderProgram, 112);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
