#pragma once

#include <functional>

namespace rml::qt::detail
{
	bool connect_function(const void* sender, void* signal_addr, const void* sender_meta, std::function<void(void**)> slot);

	/// Runs `slot` once from the event loop of the thread that owns `context`.
	/// Safe to call from any thread.
	bool invoke_queued(void* context, std::function<void()> slot);
}
