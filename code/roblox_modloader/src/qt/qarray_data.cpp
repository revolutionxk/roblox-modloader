#include "RobloxModLoader/qt/qarray_data.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt::detail
{
	static constexpr std::size_t BLOCK_ALIGNMENT = alignof(void*);
	static constexpr int PERSISTENT = -1;

	void release_array_data(QArrayData*& d, const std::size_t element_size)
	{
		if (!d)
			return;

		static const auto deallocate = core<void (*)(QArrayData*, std::size_t, std::size_t)>(
		    "QArrayData::deallocate(QArrayData*, unsigned long, unsigned long)");

		if (!deallocate)
			return;

		const int count = d->ref.load(std::memory_order_relaxed);
		if (count == PERSISTENT || (count != 0 && d->ref.fetch_sub(1, std::memory_order_acq_rel) != 1))
		{
			d = nullptr;
			return;
		}

		deallocate(d, element_size, BLOCK_ALIGNMENT);
		d = nullptr;
	}

	static void destroy_container(QArrayData*& d, void* const destructor, const std::size_t element_size)
	{
		if (!d)
			return;

		if (destructor)
		{
			reinterpret_cast<void (*)(QArrayData**)>(destructor)(&d);
			d = nullptr;
			return;
		}

		release_array_data(d, element_size);
	}

	void destroy_qstring(QArrayData*& d)
	{
		static void* const destructor = core_export_optional("QString::~QString()");
		destroy_container(d, destructor, sizeof(char16_t));
	}

	void destroy_qbytearray(QArrayData*& d)
	{
		static void* const destructor = core_export_optional("QByteArray::~QByteArray()");
		destroy_container(d, destructor, sizeof(char));
	}
}
