#include "cloud_state.hpp"

#include "cloud_registry.hpp"

#include <RobloxModLoader/roblox/graphics/device.hpp>
#include <RobloxModLoader/roblox/graphics/global_shader_data.hpp>
#include <RobloxModLoader/roblox/graphics/texture.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace clouds
{
	using namespace RBX::Graphics;

	struct Quality
	{
		int steps;
		int light_steps;
		int octaves;
		float blend;
	};

	static constexpr float k_planet_radius = 2.27e7f;
	static constexpr float k_extinction_per_stud = 0.045f;
	static constexpr float k_noise_frequency = 1.f / 165000.f;
	static constexpr float k_weather_frequency = 1.f / 150000.f;
	static constexpr float k_camera_cut_distance = 250.f;
	static constexpr float k_detail_drift = 0.35f;
	static constexpr float k_shear = 0.35f;
	static constexpr float k_shadow_extent = 32768.f;
	static constexpr Quality k_qualities[] = {{32, 3, 2, 0.35f}, {48, 4, 2, 0.35f}, {64, 4, 3, 0.35f}, {96, 6, 3, 0.3f}};

	static void set4(float (&out)[4], const float x, const float y, const float z, const float w)
	{
		out[0] = x;
		out[1] = y;
		out[2] = z;
		out[3] = w;
	}

	static RBX::Color3 linear(const RBX::Color3& c)
	{
		return RBX::Color3(c.r * c.r, c.g * c.g, c.b * c.b);
	}

	static RBX::Vector3 xyz(const RBX::Vector4& v)
	{
		return RBX::Vector3(v.x, v.y, v.z);
	}

	static Matrix to_matrix(const RBX::Matrix4& source)
	{
		Matrix result{};
		const float* values = source;
		std::copy(values, values + 16, result.begin());
		return result;
	}

	static Matrix multiply(const Matrix& a, const Matrix& b)
	{
		Matrix result{};
		for (int row = 0; row < 4; ++row)
			for (int column = 0; column < 4; ++column)
				for (int k = 0; k < 4; ++k)
					result[row * 4 + column] += a[row * 4 + k] * b[k * 4 + column];
		return result;
	}

	static Matrix translation(const RBX::Vector3& v)
	{
		return {1, 0, 0, v.x, 0, 1, 0, v.y, 0, 0, 1, v.z, 0, 0, 0, 1};
	}

	static Matrix inverse(const Matrix& a)
	{
		std::array<double, 16> m{};
		for (std::size_t i = 0; i < 16; ++i)
			m[i] = a[i];

		std::array<double, 16> inv{};
		inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
		inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
		inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
		inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
		inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
		inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
		inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
		inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
		inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
		inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
		inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
		inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
		inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
		inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
		inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
		inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

		const double determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
		Matrix result{};
		if (std::abs(determinant) < 1e-30)
			return result;
		for (std::size_t i = 0; i < 16; ++i)
			result[i] = static_cast<float>(inv[i] / determinant);
		return result;
	}

	static RBX::Vector3 wind_velocity(const CloudSettings& settings)
	{
		if (settings.use_global_wind)
			return RBX::Vector3(settings.global_wind.x, 0.f, settings.global_wind.z) * settings.global_wind_scale;

		const RBX::Vector3 direction(settings.wind_direction.x, 0.f, settings.wind_direction.z);
		const float length = direction.length();
		return length > 1e-4f ? direction * (settings.wind_speed / length) : RBX::Vector3(0.f, 0.f, 0.f);
	}

	static std::shared_ptr<Texture> upload(Device& device, const NoiseTexture& noise, const Texture::Type type, const Texture::Format format, const std::string& name)
	{
		const auto mips = static_cast<unsigned>(noise.mips.size());
		auto texture = device.create_texture_impl(type, format, noise.size, noise.size, noise.depth, mips, 1, 1, Texture::Usage::ShaderRead, name);
		if (!texture)
			return nullptr;
		for (unsigned mip = 0; mip < mips; ++mip)
		{
			const auto size = std::max(noise.size >> mip, 1u);
			const auto depth = std::max(noise.depth >> mip, 1u);
			texture->upload(0, mip, TextureRegion{0, 0, 0, size, size, depth}, noise.mips[mip].data(), static_cast<unsigned>(noise.mips[mip].size()));
		}
		return texture;
	}

	CloudState::CloudState(std::filesystem::path cache_file, std::shared_ptr<spdlog::logger> log) :
	    m_log(std::move(log))
	{
		m_noise_future = std::async(std::launch::async, [path = std::move(cache_file), log = m_log] {
			const auto start = std::chrono::steady_clock::now();
			auto noise = load_or_generate_noise(path);
			log->info("Noise volumes ready in {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
			return noise;
		});
	}

	bool CloudState::ensure_noise(Device& device)
	{
		if (m_shape && m_detail && m_weather)
			return true;
		if (!m_noise)
		{
			if (!m_noise_future.valid() || m_noise_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				return false;
			m_noise = m_noise_future.get();
		}
		if (!m_shape)
			m_shape = upload(device, m_noise->shape, Texture::Type::Type_3D, Texture::Format::R8, "rml_clouds_shape");
		if (!m_detail)
			m_detail = upload(device, m_noise->detail, Texture::Type::Type_3D, Texture::Format::R8, "rml_clouds_detail");
		if (!m_weather)
			m_weather = upload(device, m_noise->weather, Texture::Type::Type_2D, Texture::Format::RG8, "rml_clouds_weather");
		return m_shape && m_detail && m_weather;
	}

	void CloudState::begin_frame(const rml::render::FrameContext& frame)
	{
		const bool was_active = std::exchange(m_was_active, false);
		m_active = false;
		if (frame.capture || !frame.device || !frame.globals)
			return;

		const auto settings = CloudRegistry::instance().active();
		if (!settings)
		{
			m_logged = false;
			return;
		}

		if (m_device != frame.device)
			device_lost();
		m_device = frame.device;
		try
		{
			if (!ensure_noise(*frame.device))
				return;
		}
		catch (const std::exception& e)
		{
			m_log->error("Cloud noise upload failed: {}", e.what());
			return;
		}

		m_settings = *settings;
		m_width = frame.render_width;
		m_height = frame.render_height;
		if (!was_active)
			m_history_valid = false;
		m_active = true;
		m_was_active = true;
	}

	void CloudState::update(const rml::render::FrameContext& frame, const Texture* depth)
	{
		const auto& globals = *frame.globals;
		const auto& settings = m_settings;
		const auto& quality = k_qualities[std::clamp(settings.quality, 1, 4) - 1];

		const auto now = std::chrono::steady_clock::now();
		const float dt = m_last_time ? std::clamp(std::chrono::duration<float>(now - *m_last_time).count(), 0.f, 0.1f) : 0.f;
		m_last_time = now;

		const RBX::Vector3 camera = xyz(globals.camera_position[0]);
		const RBX::Vector3 view_dir = xyz(globals.view_dir);
		const Matrix view_proj = multiply(to_matrix(globals.view_projection[0]), translation(camera));
		const bool cut = !m_prev_view_proj || (camera - m_prev_camera).length() > k_camera_cut_distance || view_dir.dot(m_prev_view_dir) < 0.5f;
		const Matrix prev_view_proj = m_prev_view_proj ? *m_prev_view_proj : view_proj;
		const RBX::Vector3 delta = m_prev_view_proj ? camera - m_prev_camera : RBX::Vector3(0, 0, 0);

		const RBX::Vector3 velocity = wind_velocity(settings);
		const float speed = velocity.length();
		const RBX::Vector3 wind_dir = speed > 1e-4f ? velocity / speed : RBX::Vector3(0, 0, 0);
		m_shape_offset[0] += velocity.x * dt;
		m_shape_offset[1] += velocity.z * dt;
		m_detail_offset[0] += velocity.x * k_detail_drift * dt;
		m_detail_offset[1] += velocity.z * k_detail_drift * dt;
		m_shape_evolution += settings.evolution * (3.0 + speed * 0.05) * dt;
		m_detail_evolution += settings.evolution * (5.0 + speed * 0.06) * dt;
		const float shear = settings.thickness * k_shear * std::min(speed / 60.f, 1.f);

		RBX::Vector3 sun = -xyz(globals.lamp0_dir);
		sun = sun.length() > 1e-4f ? sun / sun.length() : RBX::Vector3(0, 1, 0);
		const auto seed = static_cast<std::uint32_t>(settings.seed) * 2654435761u;
		const float seed_x = static_cast<float>(seed % 100003u) * 1.37f;
		const float seed_z = static_cast<float>((seed >> 16) % 100019u) * 1.91f;
		const RBX::Color3 albedo = linear(settings.color);
		const RBX::Vector4& sky_ambient = globals.ambient_color;
		const RBX::Vector4& fog = globals.fog_color_global_force_field_time;
		const float ambient = settings.ambient_intensity;
		const float sun_scale = settings.sun_intensity;
		const float ms = 0.1f + 0.75f * settings.multi_scattering;
		const float width = static_cast<float>(m_width);
		const float height = static_cast<float>(m_height);
		const float history_width = static_cast<float>((m_width + 1) / 2);
		const float history_height = static_cast<float>((m_height + 1) / 2);

		auto& f = m_frame;
		f.inv_view_proj = inverse(view_proj);
		f.prev_view_proj = prev_view_proj;
		f.view_proj = view_proj;
		set4(f.camera_pos, camera.x, camera.y, camera.z, 0.5f / k_planet_radius);
		set4(f.camera_delta, delta.x, delta.y, delta.z, 0.f);
		set4(f.sun_dir, sun.x, sun.y, sun.z, 0.f);
		set4(f.sun_color, globals.lamp0_color.x * sun_scale, globals.lamp0_color.y * sun_scale, globals.lamp0_color.z * sun_scale, 1.f);
		set4(f.ambient_top, (sky_ambient.x + fog.x) * 0.18f * ambient, (sky_ambient.y + fog.y) * 0.18f * ambient, (sky_ambient.z + fog.z) * 0.18f * ambient, 0.f);
		set4(f.ambient_bottom, sky_ambient.x * 0.35f * ambient, sky_ambient.y * 0.35f * ambient, sky_ambient.z * 0.35f * ambient, 0.f);
		set4(f.fog_color, fog.x, fog.y, fog.z, settings.horizon_fade);
		set4(f.layer, settings.base_altitude, settings.base_altitude + settings.thickness, settings.thickness, 1.f / settings.thickness);
		set4(f.shape, settings.coverage, 2.f * settings.density * settings.density * k_extinction_per_stud, settings.shape_factor, settings.shape_scale * k_noise_frequency);
		set4(f.erosion, settings.erosion_factor, settings.erosion_scale * k_noise_frequency, settings.cloud_type, settings.powder);
		set4(f.wind, static_cast<float>(m_shape_offset[0]), static_cast<float>(m_shape_offset[1]), static_cast<float>(m_detail_offset[0]), static_cast<float>(m_detail_offset[1]));
		set4(f.weather, seed_x + static_cast<float>(m_shape_offset[0]), seed_z + static_cast<float>(m_shape_offset[1]), k_weather_frequency, ms);
		set4(f.albedo, albedo.r, albedo.g, albedo.b, 0.f);
		set4(f.history_size, history_width, history_height, 1.f / history_width, 1.f / history_height);
		set4(f.screen_size, width, height, 1.f / width, 1.f / height);
		set4(f.params, 0.f, static_cast<float>(quality.steps), static_cast<float>(quality.light_steps), static_cast<float>(quality.octaves));
		set4(f.temporal, m_history_valid && !cut ? 1.f : 0.f, quality.blend, 0.f, 0.f);
		const float depth_width = depth ? static_cast<float>(std::min(depth->width, m_width)) : width;
		const float depth_height = depth ? static_cast<float>(std::min(depth->height, m_height)) : height;
		set4(f.depth_info, depth_width, depth_height, std::max(settings.horizon_fade * 3.f, 50000.f), 0.f);
		set4(f.motion, static_cast<float>(m_shape_evolution), static_cast<float>(m_detail_evolution), wind_dir.x * shear, wind_dir.z * shear);
		const float shadow_texel = k_shadow_extent / static_cast<float>(k_shadow_size);
		set4(f.shadow, std::floor(camera.x / shadow_texel) * shadow_texel, std::floor(camera.z / shadow_texel) * shadow_texel, 1.f / k_shadow_extent, settings.shadow_strength);
		set4(f.advect, velocity.x * dt, 0.f, velocity.z * dt, 0.f);

		m_prev_view_proj = view_proj;
		m_prev_camera = camera;
		m_prev_view_dir = view_dir;

		if (!m_logged)
		{
			m_logged = true;
			m_log->info("Clouds active: {}x{}, quality {}, depth samples {}", m_width, m_height, settings.quality, depth ? depth->samples : 0);
		}
	}

	void CloudState::commit_history()
	{
		m_history_index ^= 1u;
		m_history_valid = true;
	}

	void CloudState::invalidate_history()
	{
		m_history_valid = false;
	}

	void CloudState::device_lost()
	{
		m_shape.reset();
		m_detail.reset();
		m_weather.reset();
		m_device = nullptr;
		m_history_valid = false;
	}

	bool CloudState::active() const
	{
		return m_active;
	}

	bool CloudState::shadows() const
	{
		return m_active && m_settings.shadow_strength > 0.f;
	}

	const CloudFrame& CloudState::constants() const
	{
		return m_frame;
	}

	Texture* CloudState::shape() const
	{
		return m_shape.get();
	}

	Texture* CloudState::detail() const
	{
		return m_detail.get();
	}

	Texture* CloudState::weather() const
	{
		return m_weather.get();
	}

	std::uint32_t CloudState::history_read() const
	{
		return m_history_index;
	}

	std::uint32_t CloudState::history_write() const
	{
		return m_history_index ^ 1u;
	}

	rml::render::TargetDesc CloudState::half(const std::initializer_list<Texture::Format> colors) const
	{
		rml::render::TargetDesc desc;
		desc.width = (m_width + 1) / 2;
		desc.height = (m_height + 1) / 2;
		desc.color_count = static_cast<std::uint8_t>(std::min<std::size_t>(colors.size(), desc.colors.size()));
		std::copy_n(colors.begin(), desc.color_count, desc.colors.begin());
		return desc;
	}

	rml::render::TargetDesc CloudState::shadow_desc()
	{
		rml::render::TargetDesc desc;
		desc.width = k_shadow_size;
		desc.height = k_shadow_size;
		desc.colors[0] = Texture::Format::R16F;
		return desc;
	}

	spdlog::logger& CloudState::log() const
	{
		return *m_log;
	}
}
