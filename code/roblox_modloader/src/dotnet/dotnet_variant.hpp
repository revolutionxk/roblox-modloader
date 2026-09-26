#pragma once
#include "RobloxModLoader/util/memory.hpp"
#include "interop_registry.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rml::dotnet
{
	using InteropStringPool = std::vector<char*>;

	[[nodiscard]] inline InteropVariant null_value() noexcept
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Null;
		out.as_uint64 = 0;
		return out;
	}

	enum class InstanceOwnership : std::uint8_t
	{
		Borrowed,
		Transferred
	};

	[[nodiscard]] inline InteropVariant instance_value(const uintptr_t ptr) noexcept
	{
		if (!utils::memory::is_valid_pointer(ptr))
			return null_value();

		InteropVariant out{};
		out.tag = InteropValueTag::Instance;
		out.as_instance = ptr;
		return out;
	}

	[[nodiscard]] inline InteropVariant bool_value(const bool value) noexcept
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Bool;
		out.as_bool = value;
		return out;
	}

	[[nodiscard]] inline InteropVariant int64_value(const int64_t value) noexcept
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Int64;
		out.as_int64 = value;
		return out;
	}

	[[nodiscard]] inline InteropVariant float_value(const float value) noexcept
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Float;
		out.as_float = value;
		return out;
	}

	[[nodiscard]] inline InteropVariant double_value(const double value) noexcept
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Double;
		out.as_double = value;
		return out;
	}

	[[nodiscard]] inline InteropVariant string_value(const char* text)
	{
		InteropVariant out{};
		out.tag = InteropValueTag::String;
		out.as_string = strdup(text ? text : "");
		return out;
	}

	[[nodiscard]] inline InteropVariant string_value(const char* text, InteropStringPool& pool)
	{
		const auto out = string_value(text);
		if (out.as_string)
			pool.push_back(out.as_string);
		return out;
	}

	[[nodiscard]] inline InteropVariant tuple_value(const std::vector<InteropVariant>& values)
	{
		if (values.empty())
			return null_value();

		const size_t count = values.size();
		const size_t buf_size = sizeof(uint64_t) + count * sizeof(InteropVariant);

		auto* buf = static_cast<std::byte*>(std::malloc(buf_size));
		if (!buf)
			return null_value();

		*reinterpret_cast<uint64_t*>(buf) = count;
		std::memcpy(buf + sizeof(uint64_t), values.data(), count * sizeof(InteropVariant));

		InteropVariant out{};
		out.tag = InteropValueTag::Tuple;
		out.as_instance = reinterpret_cast<uintptr_t>(buf);
		return out;
	}

	[[nodiscard]] inline InteropVariant blittable_value(const void* bytes, const size_t size)
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Blittable;
		out.as_instance = 0;
		if (bytes && size)
		{
			if (auto* buf = std::malloc(size))
			{
				std::memcpy(buf, bytes, size);
				out.as_instance = reinterpret_cast<uintptr_t>(buf);
			}
		}
		return out;
	}

	struct engine_vector_header
	{
		const std::byte* begin;
		const std::byte* end;
		const std::byte* capacity;
	};

	[[nodiscard]] inline InteropVariant pack_sequence(const void* vec_storage, const size_t stride)
	{
		InteropVariant out{};
		out.tag = InteropValueTag::Blittable;
		out.as_instance = 0;

		const auto* header = static_cast<const engine_vector_header*>(vec_storage);
		const size_t bytes = (header->end > header->begin) ? static_cast<size_t>(header->end - header->begin) : 0;
		const auto count = static_cast<int32_t>(bytes / stride);

		const size_t buf_size = sizeof(int32_t) + bytes;
		if (auto* buf = static_cast<std::byte*>(std::malloc(buf_size)))
		{
			*reinterpret_cast<int32_t*>(buf) = count;
			if (bytes)
				std::memcpy(buf + sizeof(int32_t), header->begin, bytes);
			out.as_instance = reinterpret_cast<uintptr_t>(buf);
		}
		return out;
	}

	[[nodiscard]] inline bool read_bool(const InteropVariant& v, bool& out) noexcept
	{
		switch (v.tag)
		{
		case InteropValueTag::Bool: out = v.as_bool; return true;
		case InteropValueTag::Int64: out = v.as_int64 != 0; return true;
		default: return false;
		}
	}

	[[nodiscard]] inline bool read_int64(const InteropVariant& v, int64_t& out) noexcept
	{
		switch (v.tag)
		{
		case InteropValueTag::Int64: out = v.as_int64; return true;
		case InteropValueTag::Bool: out = v.as_bool ? 1 : 0; return true;
		default: return false;
		}
	}

	[[nodiscard]] inline bool read_double(const InteropVariant& v, double& out) noexcept
	{
		switch (v.tag)
		{
		case InteropValueTag::Double: out = v.as_double; return true;
		case InteropValueTag::Float: out = v.as_float; return true;
		case InteropValueTag::Int64: out = static_cast<double>(v.as_int64); return true;
		default: return false;
		}
	}

	[[nodiscard]] inline const char* read_string(const InteropVariant& v) noexcept
	{
		return v.tag == InteropValueTag::String ? v.as_string : nullptr;
	}

	template<typename T>
	[[nodiscard]] T* read_instance(const InteropVariant& v) noexcept
	{
		return v.tag == InteropValueTag::Instance ? reinterpret_cast<T*>(v.as_instance) : nullptr;
	}

	template<typename T>
	[[nodiscard]] const T* read_struct_ptr(const InteropVariant& v) noexcept
	{
		return (v.tag == InteropValueTag::Blittable || v.tag == InteropValueTag::Instance) ?
		    reinterpret_cast<const T*>(v.as_instance) :
		    nullptr;
	}
} // namespace rml::dotnet
