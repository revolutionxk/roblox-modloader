#pragma once

#include "RobloxModLoader/util/compile_time_helpers.hpp"
#include "anchor.hpp"

#include <cstddef>

namespace rml::memory
{
	struct signature
	{
		cstxpr_capped_str<64> m_name;
		cstxpr_capped_str<768> m_ida;
		void (*m_on_signature_found)(memory::handle ptr){};
		anchor_path m_anchor;

		constexpr signature() = default;

		template<std::size_t N, std::size_t M>
		consteval signature(const char (&name)[N], const char (&ida)[M], void (*on_signature_found)(memory::handle ptr)) :
		    m_name(name),
		    m_ida(ida),
		    m_on_signature_found(on_signature_found)
		{
		}

		template<std::size_t N>
		consteval signature(const char (&name)[N], const anchor_path& anchor, void (*on_signature_found)(memory::handle ptr)) :
		    m_name(name),
		    m_on_signature_found(on_signature_found),
		    m_anchor(anchor)
		{
		}

		[[nodiscard]] constexpr bool anchored() const
		{
			return !m_anchor.empty();
		}
	};
}
