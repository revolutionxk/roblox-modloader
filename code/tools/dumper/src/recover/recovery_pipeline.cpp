#include "rml/dumper/recover/recovery_pipeline.hpp"

#include "recover/call_info_recoverer.hpp"
#include "recover/closure_recoverer.hpp"
#include "recover/common_header_recoverer.hpp"
#include "recover/global_state_recoverer.hpp"
#include "recover/lua_debug_recoverer.hpp"
#include "recover/lua_state_recoverer.hpp"
#include "recover/proto_recoverer.hpp"
#include "recover/tstring_recoverer.hpp"
#include "recover/tvalue_recoverer.hpp"

#include <algorithm>
#include <format>
#include <set>

namespace rml::dumper::recover
{
	RecoveryPipeline::RecoveryPipeline(std::vector<std::unique_ptr<Recoverer>> recoverers) :
	    m_recoverers(std::move(recoverers))
	{
	}

	std::expected<std::vector<const Recoverer*>, Error> RecoveryPipeline::order() const
	{
		std::set<std::string_view> known;
		for (const auto& recoverer : m_recoverers)
			known.insert(recoverer->struct_name());

		for (const auto& recoverer : m_recoverers)
			for (const auto& dependency : recoverer->depends_on())
				if (!known.contains(dependency))
					return std::unexpected(Error::make(ErrorCode::recovery, "{} depends on unknown {}",
					                                   recoverer->struct_name(), dependency));

		std::vector<const Recoverer*> ordered;
		std::set<std::string_view> placed;

		while (ordered.size() < m_recoverers.size())
		{
			const auto before = ordered.size();

			for (const auto& recoverer : m_recoverers)
			{
				if (placed.contains(recoverer->struct_name()))
					continue;

				const auto ready = std::ranges::all_of(recoverer->depends_on(), [&placed](const auto& name) {
					return placed.contains(name);
				});

				if (!ready)
					continue;

				ordered.push_back(recoverer.get());
				placed.insert(recoverer->struct_name());
			}

			if (ordered.size() == before)
			{
				std::string stuck;
				for (const auto& recoverer : m_recoverers)
					if (!placed.contains(recoverer->struct_name()))
						stuck += std::format(" {}", recoverer->struct_name());

				return std::unexpected(Error::make(ErrorCode::recovery, "dependency cycle between{}", stuck));
			}
		}

		return ordered;
	}

	std::expected<schema::LayoutSet, Error> RecoveryPipeline::run(RecoveryContext& context) const
	{
		const auto ordered = order();
		if (!ordered)
			return std::unexpected(ordered.error());

		schema::LayoutSet layouts;

		for (const auto* recoverer : *ordered)
		{
			auto layout = recoverer->recover(context);
			if (!layout)
				return std::unexpected(layout.error());

			if (const auto valid = validate(*layout); !valid)
				return std::unexpected(valid.error());

			context.publish(*layout);
			layouts.structs.insert_or_assign(layout->name, std::move(*layout));
		}

		layouts.report = context.report();

		return layouts;
	}

	RecoveryPipeline RecoveryPipeline::make_default()
	{
		std::vector<std::unique_ptr<Recoverer>> recoverers;
		recoverers.push_back(std::make_unique<CommonHeaderRecoverer>());
		recoverers.push_back(std::make_unique<LuaStateRecoverer>());
		recoverers.push_back(std::make_unique<ClosureRecoverer>());
		recoverers.push_back(std::make_unique<ProtoRecoverer>());
		recoverers.push_back(std::make_unique<TStringRecoverer>());
		recoverers.push_back(std::make_unique<CallInfoRecoverer>());
		recoverers.push_back(std::make_unique<TValueRecoverer>());
		recoverers.push_back(std::make_unique<LuaDebugRecoverer>());
		recoverers.push_back(std::make_unique<GlobalStateRecoverer>());

		return RecoveryPipeline(std::move(recoverers));
	}
}
