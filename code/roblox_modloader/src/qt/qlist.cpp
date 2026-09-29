#include "RobloxModLoader/qt/qlist.hpp"

#include "RobloxModLoader/qt/qt_module.hpp"

namespace rml::qt::detail
{
	static constexpr int PERSISTENT = -1;

	void release_list_data(QListData::Data* d) noexcept
	{
		if (!d || d->ref.load(std::memory_order_relaxed) == PERSISTENT || d->ref.fetch_sub(1, std::memory_order_acq_rel) != 1)
			return;

		static const auto dispose = core<void (*)(QListData::Data*)>("QListData::dispose(QListData::Data*)");
		if (dispose)
			dispose(d);
	}
}
