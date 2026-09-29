#include "RobloxModLoader/qt/qlist.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt::detail
{
	static constexpr int PERSISTENT = -1;
	static constexpr int UNSHARABLE = 0;

	void release_list_data(QListData::Data* d) noexcept
	{
		if (!d)
			return;

		const int count = d->ref.load(std::memory_order_relaxed);
		if (count == PERSISTENT || (count != UNSHARABLE && d->ref.fetch_sub(1, std::memory_order_acq_rel) != 1))
			return;

		static const auto dispose = core<void (*)(QListData::Data*)>("QListData::dispose(QListData::Data*)");
		if (dispose)
			dispose(d);
	}
}
