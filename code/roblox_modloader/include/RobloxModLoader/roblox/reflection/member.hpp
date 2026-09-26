#pragma once

#include "RobloxModLoader/roblox/reflection/array_view.hpp"
#include "RobloxModLoader/roblox/security/script_permissions.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "descriptor.hpp"

#include <cstring>
#include <string_view>
#include <vector>

namespace RBX::Reflection
{
	class ClassDescriptor;

	struct StringHashPredicate
	{
		size_t operator()(const char* s) const;
	};

	struct StringEqualPredicate
	{
		bool operator()(const char* lhs, const char* rhs) const
		{
			return strcmp(lhs, rhs) == 0;
		}
	};

	template<typename MemberDescriptorType>
	class MemberDescriptorContainer
	{
	public:
		struct Entry
		{
			MemberDescriptorType* descriptor;
			std::uint32_t kind;
			std::uint32_t reserved_c;
		};

		struct ConstIterator
		{
			const Entry* ptr;

			MemberDescriptorType* operator*() const noexcept
			{
				return ptr->descriptor;
			}
			ConstIterator& operator++() noexcept
			{
				++ptr;
				return *this;
			}
			bool operator!=(const ConstIterator& o) const noexcept
			{
				return ptr != o.ptr;
			}
		};
		using Iterator = ConstIterator;

		struct DescriptorView
		{
			ConstIterator m_begin, m_end;
			ConstIterator begin() const noexcept
			{
				return m_begin;
			}
			ConstIterator end() const noexcept
			{
				return m_end;
			}
			[[nodiscard]] std::size_t size() const noexcept
			{
				return static_cast<std::size_t>(m_end.ptr - m_begin.ptr);
			}
			[[nodiscard]] bool empty() const noexcept
			{
				return m_begin.ptr == m_end.ptr;
			}
		};

		std::vector<RBX::ArrayView<const MemberDescriptorType*>> views;
		const Entry* finalized_data;
		std::size_t finalized_size;
		std::uint64_t total;
		MemberDescriptorContainer* base_container;
#if defined(RML_WINDOWS)
		std::vector<RBX::ArrayView<const MemberDescriptorType*>> finalize_scratch;
#endif
		void* owner;
		std::uint8_t finalized;
		std::byte reserved_41[7];

		DescriptorView get_descriptor_view() const noexcept
		{
			return {descriptors_begin(), descriptors_end()};
		}

		ConstIterator descriptors_begin() const noexcept
		{
			return {finalized ? finalized_data : nullptr};
		}
		ConstIterator descriptors_end() const noexcept
		{
			return {finalized ? finalized_data + finalized_size : nullptr};
		}

		std::size_t descriptor_size() const
		{
			return finalized ? finalized_size : 0;
		}

		MemberDescriptorType* find_descriptor(const char* name) const
		{
			if (finalized)
			{
				for (auto* descriptor : get_descriptor_view())
				{
					if (descriptor && descriptor->name == name)
						return descriptor;
				}
				return nullptr;
			}

			for (const auto* container = this; container; container = container->base_container)
			{
				for (const auto& view : container->views)
				{
					for (const auto* descriptor : view)
					{
						if (descriptor && descriptor->name == name)
							return const_cast<MemberDescriptorType*>(descriptor);
					}
				}
			}
			return nullptr;
		}

		ConstIterator members_begin(const void*) const
		{
			return descriptors_begin();
		}
		ConstIterator members_end(const void*) const
		{
			return descriptors_end();
		}
		Iterator members_begin(void*) const
		{
			return descriptors_begin();
		}
		Iterator members_end(void*) const
		{
			return descriptors_end();
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(MemberDescriptorContainer<ClassDescriptor>::Entry, 0x10);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, finalized_data, 0x18);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, total, 0x28);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, base_container, 0x30);
#if defined(RML_WINDOWS)
	RML_ASSERT_SIZE(MemberDescriptorContainer<ClassDescriptor>, 0x60);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, owner, 0x50);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, finalized, 0x58);
#else
	RML_ASSERT_SIZE(MemberDescriptorContainer<ClassDescriptor>, 0x48);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, owner, 0x38);
	RML_ASSERT_OFFSET(MemberDescriptorContainer<ClassDescriptor>, finalized, 0x40);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()

		class MemberDescriptor : public Descriptor
	{
	public:
		static void (*member_hiding_hook)(MemberDescriptor*, MemberDescriptor*);

		const Name& category;
		const ClassDescriptor& owner;
		const Security::Permissions security;
		std::uint64_t reserved_40;

		MemberDescriptor() = delete;

	protected:
		virtual ~MemberDescriptor() = default;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(MemberDescriptor, 0x48);
	RML_ASSERT_REF_OFFSET(MemberDescriptor, category, 0x28);
	RML_ASSERT_REF_OFFSET(MemberDescriptor, owner, 0x30);
	RML_ASSERT_OFFSET(MemberDescriptor, security, 0x38);
	RML_LAYOUT_DIAGNOSTIC_POP()
}