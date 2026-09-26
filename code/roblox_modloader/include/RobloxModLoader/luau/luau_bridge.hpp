#pragma once

#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/luau/dispatch/value.hpp"
#include <unordered_map>

namespace rml::luau
{
	class ScriptRuntime;
	class Bridge;

	struct BridgeTable;

	using BridgeValue = std::variant<std::monostate, bool, double, std::string, std::shared_ptr<const BridgeTable>>;

	struct BridgeTable
	{
		std::unordered_map<std::string, BridgeValue> fields;

		[[nodiscard]] bool has(const std::string& key) const { return fields.contains(key); }

		template <typename T>
		[[nodiscard]] T get(const std::string& key, T fallback = T{}) const
		{
			const auto it = fields.find(key);
			if (it == fields.end())
			{
				return fallback;
			}

			if (const auto* held = std::get_if<T>(&it->second))
			{
				return *held;
			}

			return fallback;
		}

		[[nodiscard]] std::shared_ptr<const BridgeTable> table(const std::string& key) const
		{
			return get<std::shared_ptr<const BridgeTable>>(key, nullptr);
		}
	};

	using BridgeArgs = std::vector<BridgeValue>;
	using NativeFunction = std::function<BridgeArgs(const BridgeArgs&)>;
	using EventHandler = std::function<void(const BridgeArgs&)>;

	template <typename T>
	using BridgeResult = std::expected<T, std::string>;

	class RML_EXPORT EventSubscription final
	{
	public:
		EventSubscription() = default;
		EventSubscription(Bridge* owner, std::string event, std::size_t id) noexcept;
		~EventSubscription();

		EventSubscription(const EventSubscription&) = delete;
		EventSubscription& operator=(const EventSubscription&) = delete;
		EventSubscription(EventSubscription&& other) noexcept;
		EventSubscription& operator=(EventSubscription&& other) noexcept;

		void unsubscribe() noexcept;
		[[nodiscard]] bool valid() const noexcept { return m_owner != nullptr; }

	private:
		Bridge* m_owner{nullptr};
		std::string m_event;
		std::size_t m_id{0};
	};

	class RML_EXPORT Bridge final
	{
	public:
		explicit Bridge(ScriptRuntime& runtime) noexcept;
		~Bridge();

		Bridge(const Bridge&) = delete;
		Bridge& operator=(const Bridge&) = delete;
		Bridge(Bridge&&) = delete;
		Bridge& operator=(Bridge&&) = delete;

		BridgeResult<void> register_function(std::string_view mod_name, std::string_view function_name,
		                                     NativeFunction callback);
		BridgeResult<void> unregister_function(std::string_view mod_name, std::string_view function_name);

		BridgeResult<void> set_shared(std::string_view key, BridgeValue value);
		[[nodiscard]] BridgeResult<BridgeValue> get_shared(std::string_view key) const;

		[[nodiscard]] BridgeResult<EventSubscription> listen(std::string_view event_name, EventHandler handler);
		BridgeResult<void> emit(std::string_view event_name, const BridgeArgs& args);

		[[nodiscard]] std::vector<std::string> registered_mods() const;
		[[nodiscard]] std::vector<std::string> registered_functions(std::string_view mod_name) const;

		[[nodiscard]] std::optional<NativeFunction> find_function(const std::string& qualified_name) const;

		void add_script_listener(RBX::DataModelType context, std::string_view event_name, RefId callback);
		void drop_script_listeners(RBX::DataModelType context) noexcept;
		void drop_script_callbacks(RBX::DataModelType context, std::span<const RefId> callbacks) noexcept;

		void remove_listener(std::string_view event_name, std::size_t id) noexcept;

	private:
		struct Listener
		{
			std::size_t id{};
			EventHandler handler;
		};

		struct ScriptListener
		{
			RBX::DataModelType context{};
			RefId callback{kInvalidRef};
		};

		void dispatch_to_scripts(std::string_view event_name, const BridgeArgs& args);

		ScriptRuntime* m_runtime;

		mutable std::shared_mutex m_mutex;
		std::unordered_map<std::string, NativeFunction> m_functions;
		std::unordered_map<std::string, BridgeValue> m_shared;
		std::unordered_map<std::string, std::vector<Listener>> m_listeners;
		std::unordered_map<std::string, std::vector<ScriptListener>> m_script_listeners;
		std::atomic<std::size_t> m_next_listener{1};
	};
}
