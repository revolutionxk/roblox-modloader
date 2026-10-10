#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rml::dumper::schema
{
	class Provenance
	{
	public:
		struct Fixed
		{
			std::string reason;
		};

		struct Recovered
		{
			std::string anchor;
			std::string probe;
		};

		Provenance() = default;

		[[nodiscard]] static Provenance fixed(std::string reason);
		[[nodiscard]] static Provenance recovered(std::string anchor, std::string probe);

		[[nodiscard]] bool is_recovered() const;
		[[nodiscard]] const Fixed* as_fixed() const;
		[[nodiscard]] const Recovered* as_recovered() const;
		[[nodiscard]] std::string describe() const;

	private:
		std::variant<Fixed, Recovered> m_source{Fixed{"unspecified"}};
	};

	struct Parameter
	{
		std::string type;
		std::string name;
	};

	struct Signature
	{
		std::string result;
		std::vector<Parameter> parameters;
	};

	struct Field
	{
		std::string name;
		std::string type;
		std::size_t size{};
		std::size_t offset{};
		Provenance provenance;
		std::size_t count{};
		std::optional<Signature> signature;

		[[nodiscard]] std::size_t end() const { return offset + size; }
		[[nodiscard]] std::size_t element_size() const { return count != 0 ? size / count : size; }
	};
}
