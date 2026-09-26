#include "RobloxModLoader/luau/script/script_tree.hpp"

#include "RobloxModLoader/luau/script/script_asset.hpp"
#include "RobloxModLoader/util/filesystem.hpp"

RML_LOG_SCOPE("ScriptTree");

namespace rml::luau
{
	std::string_view class_name_of(const NodeClass value) noexcept
	{
		switch (value)
		{
		case NodeClass::Script: return "Script";
		case NodeClass::ModuleScript: return "ModuleScript";
		case NodeClass::Folder: break;
		}

		return "Folder";
	}

	bool class_is_a(const NodeClass value, const std::string_view queried) noexcept
	{
		// IK this is a bit of a hack, but it works for our purposes Roblox type system is weird

		if (queried == "Instance")
		{
			return true;
		}

		switch (value)
		{
		case NodeClass::Script:
			return queried == "Script" || queried == "BaseScript" || queried == "LuaSourceContainer";
		case NodeClass::ModuleScript:
			return queried == "ModuleScript" || queried == "LuaSourceContainer";
		case NodeClass::Folder: break;
		}

		return queried == "Folder";
	}

	const ScriptNode* ScriptNode::child(const std::string_view child_name) const noexcept
	{
		const auto found = std::ranges::find(children, child_name, &ScriptNode::name);
		return found == children.end() ? nullptr : *found;
	}

	const ScriptNode* ScriptNode::descendant(const std::string_view child_name) const noexcept
	{
		for (const auto* entry : children)
		{
			if (entry->name == child_name)
			{
				return entry;
			}
		}

		for (const auto* entry : children)
		{
			if (const auto* found = entry->descendant(child_name))
			{
				return found;
			}
		}

		return nullptr;
	}

	const ScriptNode* ScriptNode::ancestor(const std::string_view ancestor_name) const noexcept
	{
		for (const auto* walk = parent; walk != nullptr; walk = walk->parent)
		{
			if (walk->name == ancestor_name)
			{
				return walk;
			}
		}

		return nullptr;
	}

	bool ScriptNode::descends_from(const ScriptNode* other) const noexcept
	{
		if (other == nullptr)
		{
			return false;
		}

		for (const auto* walk = parent; walk != nullptr; walk = walk->parent)
		{
			if (walk == other)
			{
				return true;
			}
		}

		return false;
	}

	std::string ScriptNode::full_name() const
	{
		std::vector<std::string_view> segments;
		for (const auto* walk = this; walk != nullptr; walk = walk->parent)
		{
			segments.push_back(walk->name);
		}

		std::string rendered;
		for (const auto& segment : std::views::reverse(segments))
		{
			if (!rendered.empty())
			{
				rendered += '.';
			}
			rendered += segment;
		}

		return rendered;
	}

	ScriptNode& ScriptTree::adopt(std::unique_ptr<ScriptNode> node)
	{
		auto& stored = *m_nodes.emplace_back(std::move(node));
		m_by_logical.emplace(stored.logical, &stored);
		return stored;
	}

	const ScriptNode* ScriptTree::find(const std::string_view logical) const noexcept
	{
		const auto found = m_by_logical.find(std::string{logical});
		return found == m_by_logical.end() ? nullptr : found->second;
	}

	bool ScriptTree::owns(const ScriptNode* node) const noexcept
	{
		if (node == nullptr)
		{
			return false;
		}

		return std::ranges::any_of(m_nodes, [node](const auto& held) { return held.get() == node; });
	}

	class TreeBuilder final
	{
	public:
		TreeBuilder(ScriptTree& tree, const std::unordered_set<std::string>& entries) noexcept
			: m_tree(&tree), m_entries(&entries)
		{
		}

		const ScriptNode* build(const std::filesystem::path& directory, std::string name, std::string logical,
		                        const ScriptNode* parent)
		{
			std::vector<std::filesystem::path> files;
			std::vector<std::filesystem::path> directories;
			std::filesystem::path init;

			if (!list(directory, files, directories, init))
			{
				return nullptr;
			}

			auto owned = std::make_unique<ScriptNode>(ScriptNode{
			    .name = std::move(name),
			    .logical = std::move(logical),
			    .klass = init.empty() ? NodeClass::Folder : classify(init),
			    .source = init.empty() ? std::filesystem::path{} : utils::canonical_or_self(init),
			    .parent = parent,
			});

			auto& node = *owned;
			std::unordered_set<std::string> taken;

			for (const auto& file : files)
			{
				auto stem = file.stem().string();
				if (!taken.insert(stem).second)
				{
					RML_WARN("Ignoring '{}': another entry already owns the name '{}'", file.string(), stem);
					continue;
				}

				auto* leaf = &m_tree->adopt(std::make_unique<ScriptNode>(ScriptNode{
				    .name = stem,
				    .logical = std::format("{}/{}", node.logical, stem),
				    .klass = classify(file),
				    .source = utils::canonical_or_self(file),
				    .parent = &node,
				}));

				node.children.push_back(leaf);
			}

			for (const auto& child : directories)
			{
				auto stem = child.filename().string();
				if (taken.contains(stem))
				{
					RML_WARN("Ignoring '{}': another entry already owns the name '{}'", child.string(), stem);
					continue;
				}

				const auto* built = build(child, stem, std::format("{}/{}", node.logical, stem), &node);
				if (built == nullptr)
				{
					continue;
				}

				taken.insert(std::move(stem));
				node.children.push_back(built);
			}

			if (node.children.empty() && node.source.empty())
			{
				return nullptr;
			}

			std::ranges::sort(node.children, {}, &ScriptNode::name);

			return &m_tree->adopt(std::move(owned));
		}

	private:
		[[nodiscard]] NodeClass classify(const std::filesystem::path& file) const
		{
			return m_entries->contains(utils::canonical_or_self(file).generic_string()) ? NodeClass::Script
			                                                                : NodeClass::ModuleScript;
		}

		static bool list(const std::filesystem::path& directory, std::vector<std::filesystem::path>& files,
		                 std::vector<std::filesystem::path>& directories, std::filesystem::path& init)
		{
			std::error_code error;
			std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied,
			                                       error);
			if (error)
			{
				return false;
			}

			const std::filesystem::directory_iterator end;
			for (; it != end; it.increment(error))
			{
				if (error)
				{
					RML_WARN("Stopped listing '{}': {}", directory.string(), error.message());
					break;
				}

				std::error_code entry_error;
				const auto& path = it->path();

				if (it->is_directory(entry_error) && !entry_error)
				{
					directories.push_back(path);
					continue;
				}

				if (!is_script_file(path))
				{
					continue;
				}

				if (is_init_file(path))
				{
					if (init.empty())
					{
						init = path;
					}
					continue;
				}

				files.push_back(path);
			}

			std::ranges::sort(files);
			std::ranges::sort(directories);

			return true;
		}

		ScriptTree* m_tree;
		const std::unordered_set<std::string>* m_entries;
	};

	ScriptTreePtr build_script_tree(const std::filesystem::path& scripts_root, std::string root_name,
	                                const std::string_view alias,
	                                const std::span<const std::filesystem::path> entry_scripts)
	{
		std::error_code error;
		if (scripts_root.empty() || !std::filesystem::is_directory(scripts_root, error) || error)
		{
			return nullptr;
		}

		std::unordered_set<std::string> entries;
		entries.reserve(entry_scripts.size());
		for (const auto& entry : entry_scripts)
		{
			entries.insert(utils::canonical_or_self(entry).generic_string());
		}

		auto tree = std::make_shared<ScriptTree>();

		TreeBuilder builder(*tree, entries);
		const auto* root = builder.build(scripts_root, std::move(root_name), std::format("@{}", alias), nullptr);
		if (root == nullptr)
		{
			return nullptr;
		}

		tree->seal(root);

		RML_DEBUG("Built the '{}' script tree with {} node(s)", root->name, tree->size());
		return tree;
	}
}
