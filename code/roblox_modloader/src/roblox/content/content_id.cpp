#include "RobloxModLoader/roblox/content_id.hpp"

#include "RobloxModLoader/util/string.hpp"

namespace RBX
{
	ContentIdType ContentId::parse(const std::string_view id)
	{
		const auto size = id.size();
		if (size == 0)
			return ContentIdType::Null;
		if (size < 5)
			return ContentIdType::Unknown;

		if (size >= 10 && id.starts_with("rbx"))
		{
			if (id[4] == 'a')
			{
				if (id.substr(4).starts_with("ssethash://"))
					return ContentIdType::AssetHash;
				if (id.substr(4).starts_with("sset://"))
					return ContentIdType::Asset;
				if (id.substr(4).starts_with("ssetid://"))
					return ContentIdType::AssetId;
				return id.substr(4).starts_with("pp://") ? ContentIdType::App : ContentIdType::Unknown;
			}
			if (id[4] == 't')
			{
				if (id.substr(4).starts_with("emp://"))
					return ContentIdType::Temporary;
				return id.substr(4).starts_with("humb://") ? ContentIdType::Thumb : ContentIdType::Unknown;
			}
			if (size >= 23 && id.substr(3).starts_with("encryptedassetid://"))
				return ContentIdType::EncryptedAssetId;
			if (id.substr(3).starts_with("http://"))
				return ContentIdType::RbxHttp;
			if (size >= 16 && id.substr(3).starts_with("gameasset://"))
				return ContentIdType::GameAsset;
			return id.substr(3).starts_with("runtime://") ? ContentIdType::Runtime : ContentIdType::Unknown;
		}

		if (id.starts_with("http"))
			return ContentIdType::Http;
		if (size >= 8 && id.starts_with("file://"))
			return ContentIdType::File;
		return ContentIdType::Unknown;
	}

	ContentId ContentId::from_url(const std::string& url)
	{
		return ContentId(url);
	}

	ContentId ContentId::from_assets(const char* file_path)
	{
		return ContentId(std::string("rbxasset://") + file_path);
	}

	ContentId ContentId::from_asset_id(const long long asset_id)
	{
		ContentId result("rbxassetid://" + std::to_string(asset_id));
		result.set_type(ContentIdType::AssetId);
		return result;
	}

	ContentId ContentId::from_temporary_id(const long long temporary_id)
	{
		ContentId result("rbxtemp://" + std::to_string(temporary_id));
		result.set_type(ContentIdType::Temporary);
		return result;
	}

	std::string ContentId::get_asset_id() const
	{
		const auto type = get_type();
		if (type == ContentIdType::AssetId)
			return id.substr(13);

		if (type == ContentIdType::Http || type == ContentIdType::RbxHttp)
		{
			const std::string lower = rml::utils::to_lower(id);
			auto position = lower.find("id=");
			if (position != std::string::npos)
			{
				position += 3;
				auto end = lower.find('&', position);
				if (end == std::string::npos)
					end = lower.size();
				return lower.substr(position, end - position);
			}
		}

		return {};
	}

	std::string ContentId::get_asset_name() const
	{
		return get_type() == ContentIdType::GameAsset ? id.substr(15) : std::string{};
	}

	void ContentId::correct_backslash(std::string& id)
	{
		std::replace(id.begin(), id.end(), '\\', '/');
	}
}
