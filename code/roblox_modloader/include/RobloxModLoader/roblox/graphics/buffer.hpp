#pragma once

#include "resource.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace RBX::Graphics
{
	class Buffer : public Resource
	{
	public:
		enum class Type : std::uint32_t
		{
			Vertex,
			Index,
			Constant,
			Structured
		};

		enum class Usage : std::uint32_t
		{
			Static,
			Dynamic
		};

		virtual void* lock() = 0;
		virtual void unlock(unsigned size) = 0;
		virtual void upload(unsigned offset, const void* data, unsigned size) = 0;
		virtual void download_debug(unsigned offset, void* data, unsigned size) = 0;

		std::uint32_t type;
		std::uint32_t size;
		std::uint32_t element_size;
		std::uint32_t usage;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Buffer, type, 0x40);
	RML_ASSERT_OFFSET(Buffer, usage, 0x4c);
	RML_ASSERT_SIZE(Buffer, 0x50);
#else
	RML_ASSERT_OFFSET(Buffer, type, 0x38);
	RML_ASSERT_OFFSET(Buffer, usage, 0x44);
	RML_ASSERT_SIZE(Buffer, 0x48);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()

	class VertexLayout : public Resource
	{
	public:
		struct Element
		{
			unsigned stream;
			unsigned offset;
			unsigned format;
			unsigned semantic;
			unsigned semantic_index;
			std::uint8_t per_instance;
			std::uint8_t reserved_15[3];
		};
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(VertexLayout::Element, 24);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class Geometry : public Resource
	{
	public:
		enum class Primitive : std::uint32_t
		{
			Triangles,
			Lines,
			Points,
			TriangleStrip
		};
	};

	struct GeometryBatch
	{
		std::shared_ptr<Geometry> geometry;
		Geometry::Primitive primitive;
		std::uint32_t index_range_begin;
		std::uint32_t base_vertex;
		std::uint32_t count;
		std::uint32_t instance_count;
		std::uint16_t reserved_36;
		bool indexed;
		std::uint8_t reserved_39;

		GeometryBatch() = default;

		GeometryBatch(std::shared_ptr<Geometry> geometry, const Geometry::Primitive primitive, const std::uint32_t index_range_begin, const std::uint32_t base_vertex, const std::uint32_t count, const std::uint32_t instance_count, const bool indexed) :
		    geometry(std::move(geometry)),
		    primitive(primitive),
		    index_range_begin(index_range_begin),
		    base_vertex(base_vertex),
		    count(count),
		    instance_count(instance_count),
		    reserved_36(0),
		    indexed(indexed),
		    reserved_39(0)
		{
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(GeometryBatch, primitive, 16);
	RML_ASSERT_OFFSET(GeometryBatch, indexed, 38);
	RML_ASSERT_SIZE(GeometryBatch, 40);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
