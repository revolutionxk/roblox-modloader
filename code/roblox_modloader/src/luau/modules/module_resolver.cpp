#include "RobloxModLoader/luau/modules/module_resolver.hpp"

#include "RobloxModLoader/util/string.hpp"

RML_LOG_SCOPE("Modules");

namespace rml::luau
{
	namespace roots
	{
		std::filesystem::path rml_libraries(const ModEnvironment& env) noexcept
		{
			return env.rml_libraries;
		}

		std::filesystem::path mod_scripts(const ModEnvironment& env) noexcept
		{
			return env.mod_scripts();
		}
	}

	static constexpr std::array kSourceExtensions{std::string_view{".luau"}, std::string_view{".lua"}};
	static constexpr std::array kInitNames{std::string_view{"init.luau"}, std::string_view{"init.lua"}};

	static std::string join_attempts(const std::vector<std::string>& attempted)
	{
		return attempted.empty() ? std::string{"nothing"} : utils::join(attempted, ", ");
	}

	static std::string known_aliases()
	{
		return utils::join(kResolveRules | std::views::transform([](const auto& rule) { return std::format("'@{}/'", rule.alias); }), ", ");
	}

	std::string ResolveFailure::describe() const
	{
		switch (error)
		{
		case ResolveError::NoPrefix:
			return std::format("module '{}' has no prefix: start it with './', '../', '@self/' or '@rml/', or pass a "
			                   "ModuleScript instance instead",
			    specifier);
		case ResolveError::UnknownAlias:
			return std::format("module '{}' asks for the unknown alias '@{}': the known aliases are {}", specifier, alias, known_aliases());
		case ResolveError::EmptyName: return std::format("module '{}' names nothing to load", specifier);
		case ResolveError::NoRequirer:
			return std::format("module '{}' is relative, but the file asking for it is not one the loader owns: use '@self/' or "
			                   "'@rml/' here",
			    specifier);
		case ResolveError::EscapesRoot:
			return std::format("module '{}' escapes '@{}': '..' cannot climb past the root, and absolute paths are not allowed", specifier, alias);
		case ResolveError::NotAFile:
			return std::format("module '{}' is not a regular file (tried: {})", specifier, join_attempts(attempted));
		case ResolveError::IoError:
			return std::format("module '{}' could not be resolved: the root of '@{}' is unavailable", specifier, alias);
		case ResolveError::NotFound: break;
		}

		if (requirer.empty())
		{
			return std::format("module '{}' not found (tried: {})", specifier, join_attempts(attempted));
		}

		return std::format("module '{}' not found from '{}' (tried: {})", specifier, requirer, join_attempts(attempted));
	}

	struct AliasSplit
	{
		std::string alias;
		std::string_view rest;
	};

	static AliasSplit split_alias(const std::string_view aliased)
	{
		const auto slash = aliased.find('/');
		if (slash == std::string_view::npos)
		{
			return AliasSplit{.alias = utils::to_lower(aliased.substr(1)), .rest = {}};
		}

		return AliasSplit{.alias = utils::to_lower(aliased.substr(1, slash - 1)), .rest = aliased.substr(slash + 1)};
	}

	static const ResolveRule* rule_for(const std::string_view alias)
	{
		const auto found = std::ranges::find(kResolveRules, alias, &ResolveRule::alias);
		return found == kResolveRules.end() ? nullptr : &*found;
	}

	static bool is_usable_segment(const std::string_view segment)
	{
		return segment.find('\0') == std::string_view::npos && segment.find(':') == std::string_view::npos;
	}

	static bool walk_segments(const std::string_view path, std::vector<std::string>& segments)
	{
		std::size_t start = 0;

		while (start <= path.size())
		{
			auto end = path.find('/', start);
			if (end == std::string_view::npos)
			{
				end = path.size();
			}

			const auto segment = path.substr(start, end - start);

			if (segment == "..")
			{
				if (segments.empty())
				{
					return false;
				}
				segments.pop_back();
			}
			else if (!segment.empty() && segment != ".")
			{
				if (!is_usable_segment(segment))
				{
					return false;
				}
				segments.emplace_back(segment);
			}

			if (end == path.size())
			{
				break;
			}

			start = end + 1;
		}

		return true;
	}

	static std::string build_logical(const std::string_view alias, const std::vector<std::string>& segments)
	{
		return segments.empty() ? std::format("@{}", alias) : std::format("@{}/{}", alias, utils::join(segments, "/"));
	}

	static bool is_inside(const std::filesystem::path& root, const std::filesystem::path& candidate)
	{
		std::error_code ec;
		const auto relative = std::filesystem::relative(candidate, root, ec);
		if (ec || relative.empty())
		{
			return false;
		}

		const auto first = relative.begin();
		return first != relative.end() && *first != "..";
	}

	static std::vector<std::filesystem::path> candidates_for(const std::filesystem::path& target, const std::vector<std::string>& segments)
	{
		std::vector<std::filesystem::path> candidates;

		if (!segments.empty() && segments.back() != "init")
		{
			for (const auto extension : kSourceExtensions)
			{
				auto candidate = target;
				candidate += extension;
				candidates.push_back(std::move(candidate));
			}
		}

		for (const auto init : kInitNames)
		{
			candidates.push_back(target / init);
		}

		return candidates;
	}

	bool is_logical_module_path(const std::string_view text) noexcept
	{
		if (text.size() < 2 || text.front() != '@')
		{
			return false;
		}

		return rule_for(split_alias(text).alias) != nullptr;
	}

	std::expected<ResolvedModule, ResolveFailure> resolve_module(const std::string_view raw_specifier, const ModEnvironment& env, const std::string_view requirer)
	{
		ResolveFailure failure{.error = ResolveError::NoPrefix, .specifier = std::string{raw_specifier}, .requirer = std::string{requirer}};

		std::string specifier{raw_specifier};
		std::ranges::replace(specifier, '\\', '/');

		if (specifier.empty())
		{
			failure.error = ResolveError::EmptyName;
			return std::unexpected(std::move(failure));
		}

		const ResolveRule* rule = nullptr;
		std::vector<std::string> segments;
		std::string_view tail;

		if (specifier.front() == '@')
		{
			auto split = split_alias(specifier);

			rule = rule_for(split.alias);
			if (rule == nullptr)
			{
				failure.error = ResolveError::UnknownAlias;
				failure.alias = std::move(split.alias);
				return std::unexpected(std::move(failure));
			}

			tail = split.rest;
		}
		else if (specifier == "." || specifier == ".." || specifier.starts_with("./") || specifier.starts_with("../"))
		{
			if (!is_logical_module_path(requirer))
			{
				failure.error = ResolveError::NoRequirer;
				return std::unexpected(std::move(failure));
			}

			const auto split = split_alias(requirer);
			rule = rule_for(split.alias);

			if (!walk_segments(split.rest, segments))
			{
				failure.error = ResolveError::NoRequirer;
				return std::unexpected(std::move(failure));
			}

			if (!segments.empty())
			{
				segments.pop_back();
			}

			tail = specifier;
		}
		else
		{
			return std::unexpected(std::move(failure));
		}

		failure.alias = std::string{rule->alias};

		if (!walk_segments(tail, segments))
		{
			failure.error = ResolveError::EscapesRoot;
			return std::unexpected(std::move(failure));
		}

		const auto root = rule->root(env);
		if (root.empty())
		{
			failure.error = ResolveError::IoError;
			return std::unexpected(std::move(failure));
		}

		std::error_code ec;
		const auto canonical_root = std::filesystem::weakly_canonical(root, ec);
		if (ec)
		{
			failure.error = ResolveError::IoError;
			return std::unexpected(std::move(failure));
		}

		auto target = root;
		for (const auto& segment : segments)
		{
			target /= segment;
		}

		bool saw_non_file = false;

		for (const auto& candidate : candidates_for(target, segments))
		{
			failure.attempted.push_back(candidate.generic_string());

			ec.clear();
			const auto status = std::filesystem::status(candidate, ec);
			if (ec || !std::filesystem::exists(status))
			{
				continue;
			}

			if (!std::filesystem::is_regular_file(status))
			{
				saw_non_file = true;
				continue;
			}

			ec.clear();
			const auto resolved = std::filesystem::weakly_canonical(candidate, ec);
			if (ec)
			{
				failure.error = ResolveError::IoError;
				return std::unexpected(std::move(failure));
			}

			if (!is_inside(canonical_root, resolved))
			{
				RML_WARN("Rejected module '{}': '{}' resolves outside '{}'",
				    raw_specifier,
				    resolved.generic_string(),
				    canonical_root.generic_string());
				failure.error = ResolveError::EscapesRoot;
				return std::unexpected(std::move(failure));
			}

			return ResolvedModule{.id = ModuleId{resolved.generic_string()}, .logical = build_logical(rule->alias, segments)};
		}

		failure.error = saw_non_file ? ResolveError::NotAFile : ResolveError::NotFound;
		return std::unexpected(std::move(failure));
	}

	std::string logical_name_for(const std::filesystem::path& file, const ModEnvironment& env)
	{
		std::error_code ec;
		auto subject = std::filesystem::weakly_canonical(file, ec);
		if (ec)
		{
			subject = file;
		}

		for (const auto& rule : kResolveRules)
		{
			const auto root = rule.root(env);
			if (root.empty())
			{
				continue;
			}

			ec.clear();
			auto canonical_root = std::filesystem::weakly_canonical(root, ec);
			if (ec)
			{
				canonical_root = root;
			}

			if (!is_inside(canonical_root, subject))
			{
				continue;
			}

			ec.clear();
			const auto relative = std::filesystem::relative(subject, canonical_root, ec);
			if (ec || relative.empty())
			{
				continue;
			}

			auto text = relative.generic_string();

			for (const auto extension : kSourceExtensions)
			{
				if (text.ends_with(extension))
				{
					text.resize(text.size() - extension.size());
					break;
				}
			}

			if (text == "init")
			{
				text.clear();
			}
			else if (text.ends_with("/init"))
			{
				text.resize(text.size() - std::string_view{"/init"}.size());
			}

			return text.empty() ? std::format("@{}", rule.alias) : std::format("@{}/{}", rule.alias, text);
		}

		return {};
	}
}
