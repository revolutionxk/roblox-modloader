#include "RobloxModLoader/assets/roblox_mesh.hpp"

#include "RobloxModLoader/util/filesystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace rml::assets::mesh
{
	struct Float3
	{
		float x, y, z;
	};

	struct Float2
	{
		float u, v;
	};

	static Float3 cross(const Float3& a, const Float3& b)
	{
		return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}

	static Float3 normalized(const Float3& v)
	{
		const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return length > 0 ? Float3{v.x / length, v.y / length, v.z / length} : Float3{0, 1, 0};
	}

	static std::int8_t packed(const float value)
	{
		return static_cast<std::int8_t>(std::lround(std::clamp(value, -1.f, 1.f) * 127.f));
	}

	static int resolve_index(const int index, const std::size_t count)
	{
		return index < 0 ? static_cast<int>(count) + index : index - 1;
	}

	static bool parse_corner(const std::string& token, int& position, int& texcoord, int& normal)
	{
		position = texcoord = normal = 0;
		const auto first = token.find('/');
		position = std::atoi(token.substr(0, first).c_str());
		if (first == std::string::npos)
			return position != 0;
		const auto second = token.find('/', first + 1);
		const auto tex = token.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
		if (!tex.empty())
			texcoord = std::atoi(tex.c_str());
		if (second != std::string::npos)
			normal = std::atoi(token.substr(second + 1).c_str());
		return position != 0;
	}

	std::expected<Mesh, std::string> load_obj(const std::filesystem::path& path)
	{
		std::ifstream in(path);
		if (!in)
			return std::unexpected(std::format("could not open '{}'", path.string()));

		std::vector<Float3> positions;
		std::vector<Float2> texcoords;
		std::vector<Float3> normals;
		std::map<std::tuple<int, int, int>, std::uint32_t> unique;
		Mesh mesh;
		std::vector<Float3> face_normals;

		std::string line;
		while (std::getline(in, line))
		{
			std::istringstream stream(line);
			std::string keyword;
			stream >> keyword;

			if (keyword == "v")
			{
				Float3 p{};
				stream >> p.x >> p.y >> p.z;
				positions.push_back(p);
			}
			else if (keyword == "vt")
			{
				Float2 t{};
				stream >> t.u >> t.v;
				texcoords.push_back(t);
			}
			else if (keyword == "vn")
			{
				Float3 n{};
				stream >> n.x >> n.y >> n.z;
				normals.push_back(n);
			}
			else if (keyword == "f")
			{
				std::vector<std::uint32_t> corners;
				std::string token;
				while (stream >> token)
				{
					int p, t, n;
					if (!parse_corner(token, p, t, n))
						return std::unexpected(std::format("malformed face '{}' in '{}'", line, path.string()));

					const auto key = std::make_tuple(resolve_index(p, positions.size()),
					    t ? resolve_index(t, texcoords.size()) : -1,
					    n ? resolve_index(n, normals.size()) : -1);
					const auto [pi, ti, ni] = key;
					if (pi < 0 || static_cast<std::size_t>(pi) >= positions.size())
						return std::unexpected(std::format("vertex index out of range in '{}'", path.string()));

					auto it = unique.find(key);
					if (it == unique.end())
					{
						Vertex vertex{};
						vertex.px = positions[pi].x;
						vertex.py = positions[pi].y;
						vertex.pz = positions[pi].z;
						if (ti >= 0 && static_cast<std::size_t>(ti) < texcoords.size())
						{
							vertex.tu = texcoords[ti].u;
							vertex.tv = 1.f - texcoords[ti].v;
						}
						if (ni >= 0 && static_cast<std::size_t>(ni) < normals.size())
						{
							const auto n = normalized(normals[ni]);
							vertex.nx = n.x;
							vertex.ny = n.y;
							vertex.nz = n.z;
						}
						vertex.r = vertex.g = vertex.b = vertex.a = 255;
						it = unique.emplace(key, static_cast<std::uint32_t>(mesh.vertices.size())).first;
						mesh.vertices.push_back(vertex);
					}
					corners.push_back(it->second);
				}

				for (std::size_t i = 1; i + 1 < corners.size(); ++i)
					mesh.faces.push_back({corners[0], corners[i], corners[i + 1]});
			}
		}

		if (mesh.faces.empty())
			return std::unexpected(std::format("'{}' has no faces", path.string()));

		std::vector<Float3> accumulated(mesh.vertices.size(), Float3{0, 0, 0});
		for (const auto& face : mesh.faces)
		{
			const auto& a = mesh.vertices[face.a];
			const auto& b = mesh.vertices[face.b];
			const auto& c = mesh.vertices[face.c];
			const Float3 n = cross({b.px - a.px, b.py - a.py, b.pz - a.pz}, {c.px - a.px, c.py - a.py, c.pz - a.pz});
			for (const auto index : {face.a, face.b, face.c})
			{
				accumulated[index].x += n.x;
				accumulated[index].y += n.y;
				accumulated[index].z += n.z;
			}
		}

		for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
		{
			auto& vertex = mesh.vertices[i];
			if (vertex.nx == 0 && vertex.ny == 0 && vertex.nz == 0)
			{
				const auto n = normalized(accumulated[i]);
				vertex.nx = n.x;
				vertex.ny = n.y;
				vertex.nz = n.z;
			}
			const Float3 normal{vertex.nx, vertex.ny, vertex.nz};
			const Float3 helper = std::fabs(normal.y) < 0.99f ? Float3{0, 1, 0} : Float3{1, 0, 0};
			const auto tangent = normalized(cross(helper, normal));
			vertex.tx = packed(tangent.x);
			vertex.ty = packed(tangent.y);
			vertex.tz = packed(tangent.z);
			vertex.ts = 127;
		}

		return mesh;
	}

	std::vector<std::byte> encode_v2(const Mesh& mesh)
	{
		static constexpr std::string_view k_version = "version 2.00\n";

		struct Header
		{
			std::uint16_t header_size;
			std::uint8_t vertex_size;
			std::uint8_t face_size;
			std::uint32_t vertex_count;
			std::uint32_t face_count;
		};
		static_assert(sizeof(Header) == 12);

		const Header header{sizeof(Header),
		    sizeof(Vertex),
		    sizeof(Face),
		    static_cast<std::uint32_t>(mesh.vertices.size()),
		    static_cast<std::uint32_t>(mesh.faces.size())};

		std::vector<std::byte> out;
		out.resize(k_version.size() + sizeof(Header) + mesh.vertices.size() * sizeof(Vertex) + mesh.faces.size() * sizeof(Face));

		auto* cursor = out.data();
		std::memcpy(cursor, k_version.data(), k_version.size());
		cursor += k_version.size();
		std::memcpy(cursor, &header, sizeof(header));
		cursor += sizeof(header);
		std::memcpy(cursor, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
		cursor += mesh.vertices.size() * sizeof(Vertex);
		std::memcpy(cursor, mesh.faces.data(), mesh.faces.size() * sizeof(Face));
		return out;
	}

	std::expected<void, std::string> write_v2(const Mesh& mesh, const std::filesystem::path& path)
	{
		const auto bytes = encode_v2(mesh);
		if (!utils::write_file(path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())))
			return std::unexpected(std::format("could not write '{}'", path.string()));
		return {};
	}
}
