#pragma once

#include "RobloxModLoader/qt/qarray_data.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

namespace rml::qt
{
	class RML_EXPORT QByteArray
	{
	public:
		QArrayData* d{};

		QByteArray() noexcept = default;
		explicit QByteArray(std::string_view bytes);
		~QByteArray();

		QByteArray(const QByteArray&) = delete;
		QByteArray& operator=(const QByteArray&) = delete;

		QByteArray(QByteArray&& other) noexcept :
		    d(std::exchange(other.d, nullptr))
		{
		}

		[[nodiscard]] std::string_view view() const noexcept
		{
			return d ? std::string_view{d->data(), static_cast<std::size_t>(d->size)} : std::string_view{};
		}
	};

	RML_ASSERT_SIZE(QByteArray, sizeof(void*));
}
