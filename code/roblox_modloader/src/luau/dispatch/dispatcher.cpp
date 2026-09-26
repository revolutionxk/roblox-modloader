#include "RobloxModLoader/luau/dispatch/dispatcher.hpp"

#include "RobloxModLoader/luau/script_host.hpp"
#include "dotnet/dotnet_variant.hpp"
#include "dotnet/interop_registry.hpp"

RML_LOG_SCOPE("Dispatcher");

namespace rml::luau
{
	static dotnet::InteropVariant to_variant(const Value& value)
	{
		return std::visit(
		    []<typename T>(const T& held) -> dotnet::InteropVariant {
			    if constexpr (std::is_same_v<T, bool>)
			    {
				    return dotnet::bool_value(held);
			    }
			    else if constexpr (std::is_same_v<T, double>)
			    {
				    return dotnet::double_value(held);
			    }
			    else if constexpr (std::is_same_v<T, std::string>)
			    {
				    return dotnet::string_value(held.c_str());
			    }
			    else if constexpr (std::is_same_v<T, InstanceHandle>)
			    {
				    return dotnet::instance_value(held.address);
			    }
			    else if constexpr (std::is_same_v<T, LuauRefHandle>)
			    {
				    dotnet::InteropVariant out{};
				    out.tag = dotnet::InteropValueTag::LuauRef;
				    out.as_uint64 = held.id;
				    return out;
			    }
			    else
			    {
				    return dotnet::null_value();
			    }
		    },
		    value);
	}

	static vm::VmError closed_error()
	{
		return vm::VmError::unavailable("script host is shutting down");
	}

	Dispatcher::~Dispatcher()
	{
		drain_cancelled(vm::VmError::unavailable("dispatcher abandoned"));
	}

	void Dispatcher::post(Work work)
	{
		enqueue(Entry{.work = std::move(work), .completion = Completion{}});
	}

	std::future<WorkResult> Dispatcher::post_for_result(Work work)
	{
		std::promise<WorkResult> promise;
		auto future = promise.get_future();

		enqueue(Entry{.work = std::move(work), .completion = Completion{std::move(promise)}});

		return future;
	}

	void Dispatcher::post_managed(Work work, ManagedCompletion completion)
	{
		enqueue(Entry{.work = std::move(work), .completion = Completion{completion}});
	}

	void Dispatcher::enqueue(Entry entry)
	{
		{
			std::lock_guard lock(m_mutex);
			if (!m_closed)
			{
				m_incoming.push_back(std::move(entry));
				return;
			}
		}

		settle(entry.completion, std::unexpected(closed_error()));
	}

	void Dispatcher::pump(ScriptHost& host, const Budget& budget) noexcept
	{
		{
			std::lock_guard lock(m_mutex);
			if (!m_draining.empty() || m_incoming.empty())
			{
				return;
			}

			std::swap(m_incoming, m_draining);
		}

		const auto deadline = std::chrono::steady_clock::now() + budget.max_time;

		std::size_t index = 0;
		while (index < m_draining.size())
		{
			if (index >= budget.max_items)
			{
				break;
			}

			if (index != 0 && std::chrono::steady_clock::now() >= deadline)
			{
				break;
			}

			auto& entry = m_draining[index];
			auto* completion = &entry.completion;

			host.execute(entry.work, [completion](WorkResult result) {
				settle(*completion, std::move(result));
			});

			if (!std::holds_alternative<std::monostate>(entry.completion))
			{
				settle(entry.completion, std::unexpected(vm::VmError::internal("work item finished without producing a result")));
			}

			++index;
		}

		std::vector<Entry> abandoned;
		{
			std::lock_guard lock(m_mutex);

			if (index < m_draining.size())
			{
				const auto first = m_draining.begin() + static_cast<std::ptrdiff_t>(index);

				if (m_closed)
				{
					abandoned.assign(std::make_move_iterator(first), std::make_move_iterator(m_draining.end()));
				}
				else
				{
					m_incoming.insert(m_incoming.begin(),
					    std::make_move_iterator(first),
					    std::make_move_iterator(m_draining.end()));
				}
			}

			m_draining.clear();
		}

		for (auto& [_, completion] : abandoned)
		{
			settle(completion, std::unexpected(closed_error()));
		}
	}

	bool Dispatcher::has_pending() const noexcept
	{
		std::lock_guard lock(m_mutex);
		return !m_incoming.empty() || !m_draining.empty();
	}

	std::size_t Dispatcher::pending_count() const noexcept
	{
		std::lock_guard lock(m_mutex);
		return m_incoming.size() + m_draining.size();
	}

	void Dispatcher::drain_cancelled(const vm::VmError& reason) noexcept
	{
		std::vector<Entry> pending;
		{
			std::lock_guard lock(m_mutex);

			m_closed = true;

			pending.reserve(m_draining.size() + m_incoming.size());
			for (auto& entry : m_draining)
			{
				pending.push_back(std::move(entry));
			}
			for (auto& entry : m_incoming)
			{
				pending.push_back(std::move(entry));
			}

			m_draining.clear();
			m_incoming.clear();
		}

		for (auto& [_, completion] : pending)
		{
			settle(completion, std::unexpected(reason));
		}
	}

	void Dispatcher::settle(Completion& completion, WorkResult result) noexcept
	{
		if (auto* promise = std::get_if<std::promise<WorkResult>>(&completion))
		{
			try
			{
				promise->set_value(std::move(result));
			}
			catch (const std::future_error& error)
			{
				RML_ERROR("Work promise was already settled: {}", error.what());
			}

			completion = std::monostate{};
			return;
		}

		if (const auto* managed = std::get_if<ManagedCompletion>(&completion))
		{
			const auto callback = managed->callback;
			void* const state = managed->state;
			completion = std::monostate{};

			if (!callback)
			{
				return;
			}

			if (result)
			{
				const auto variant = to_variant(*result);
				callback(state, &variant, nullptr);
			}
			else
			{
				const auto message = result.error().describe();
				callback(state, nullptr, message.c_str());
			}

			return;
		}

		if (!result && !result.error().reported)
		{
			RML_ERROR("{}", result.error().describe());
		}
	}
}
