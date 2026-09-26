#include "RobloxModLoader/memory/pattern.hpp"

#include "RobloxModLoader/internal/common.hpp"

#include <charconv>

namespace rml::memory
{
	static std::optional<uint8_t> hex_nibble(const char c)
	{
		uint8_t value{};
		const auto [end, error] = std::from_chars(&c, &c + 1, value, 16);
		return error == std::errc{} && end == &c + 1 ? std::optional{value} : std::nullopt;
	}

	pattern::pattern(std::string_view ida_sig)
	{
		const auto size = ida_sig.size();
		for (std::size_t i{}; i != size; ++i)
		{
			if (ida_sig[i] == ' ')
				continue;
			bool last = (i == ida_sig.size() - 1);
			if (ida_sig[i] != '?')
			{
				if (!last)
				{
					auto c1 = hex_nibble(ida_sig[i]);
					auto c2 = hex_nibble(ida_sig[i + 1]);
					if (c1 && c2)
					{
						m_bytes.emplace_back(static_cast<uint8_t>((*c1 * 0x10) + *c2));
					}
				}
			}
			else
			{
				m_bytes.push_back({});

				// add support for double question mark sigs
				if (i + 1 != size && ida_sig[i + 1] == '?')
				{
					++i;
				}
			}
		}
	}

	pattern::pattern(const void* bytes, std::string_view mask)
	{
		const auto size = mask.size();
		for (std::size_t i{}; i != size; ++i)
		{
			if (mask[i] != '?')
				m_bytes.emplace_back(static_cast<const uint8_t*>(bytes)[i]);
			else
				m_bytes.push_back(std::nullopt);
		}
	}
}
