#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include "RobloxModLoader/rml_export.hpp"

#include <atomic>
#include <cstddef>

namespace rml::qt
{
	struct QListData
	{
		struct Data
		{
			std::atomic<int> ref;
			int alloc;
			int begin;
			int end;
			void* array[1];
		};
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QListData::Data, begin, sizeof(int) * 2);
	RML_ASSERT_OFFSET(QListData::Data, end, sizeof(int) * 3);
	RML_ASSERT_OFFSET(QListData::Data, array, sizeof(int) * 4);
	RML_LAYOUT_DIAGNOSTIC_POP()

	namespace detail
	{
		RML_EXPORT void release_list_data(QListData::Data* d) noexcept;
	}
}

namespace rml::qt
{
	template<typename T>
	class QList
	{
	public:
		class ConstIterator
		{
		public:
			ConstIterator(const QListData::Data* data, const int index) noexcept :
			    m_data(data), m_index(index)
			{
			}

			[[nodiscard]] T operator*() const noexcept
			{
				return static_cast<T>(m_data->array[m_index]);
			}

			ConstIterator& operator++() noexcept
			{
				++m_index;
				return *this;
			}

			[[nodiscard]] bool operator!=(const ConstIterator& other) const noexcept
			{
				return m_index != other.m_index;
			}

		private:
			const QListData::Data* m_data;
			int m_index;
		};

		QListData::Data* d{};

		QList() = default;
		QList(const QList&) = delete;
		QList& operator=(const QList&) = delete;

		~QList()
		{
			detail::release_list_data(d);
		}

		[[nodiscard]] int size() const noexcept
		{
			return d ? d->end - d->begin : 0;
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return size() == 0;
		}

		[[nodiscard]] T at(const int index) const noexcept
		{
			return static_cast<T>(d->array[d->begin + index]);
		}

		[[nodiscard]] ConstIterator begin() const noexcept
		{
			return ConstIterator(d, d ? d->begin : 0);
		}

		[[nodiscard]] ConstIterator end() const noexcept
		{
			return ConstIterator(d, d ? d->end : 0);
		}

	};
}
