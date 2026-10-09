#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/util/layout_assert.hpp"

namespace RBX::Graphics
{
	class RenderCamera;

	struct GlobalShaderData
	{
		Matrix4 view_projection[2];
		Vector4 view_right;
		Vector4 view_up;
		Vector4 view_dir;
		Vector4 camera_position[2];
		Vector4 ambient_color;
		Vector4 sky_ambient;
		Vector4 lamp0_color;
		Vector4 lamp0_dir;
		Vector4 lamp1_color;
		Vector4 fog_params;
		Vector4 fog_color_global_force_field_time;
		Vector4 exposure_dof_distance;
		Vector4 light_config[4];
		Vector4 shadow_matrix[3];
		Vector4 refraction_bias_fade_distance_glow_factor;
		Vector4 texture_data_shadow_info;
		Vector4 sky_gradient_top_env_diffuse;
		Vector4 sky_gradient_bottom_env_spec;
		Vector4 ambient_color_no_ibl_cube_blend;
		Vector4 sky_ambient_no_ibl;
		Vector4 ambient_cube[12];
		Vector4 cascade_sphere[4];
		Vector2 inv_viewport_wh;
		Vector2 viewport_scale;
		float debug_auth_lod_mode;
		float shadow_camera_relative;
		float hq_dist;
		float local_light_dist;
		float sun_dist;
		float hybrid_lerp_slope;
		float evsm_pos_exp;
		float evsm_neg_exp;
		float global_shadow;
		float shadow_bias;
		float packed_alpha_ref;
		float debug_flags;
		Matrix4 froxel_transform;
		Vector4 skybox_rotation[3];

		RML_EXPORT void set_camera(const RenderCamera& camera);
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(GlobalShaderData, view_right, 0x80);
	RML_ASSERT_OFFSET(GlobalShaderData, camera_position, 0xB0);
	RML_ASSERT_OFFSET(GlobalShaderData, ambient_color, 0xD0);
	RML_ASSERT_OFFSET(GlobalShaderData, lamp0_color, 0xF0);
	RML_ASSERT_OFFSET(GlobalShaderData, fog_params, 0x120);
	RML_ASSERT_OFFSET(GlobalShaderData, exposure_dof_distance, 0x140);
	RML_ASSERT_OFFSET(GlobalShaderData, texture_data_shadow_info, 0x1D0);
	RML_ASSERT_OFFSET(GlobalShaderData, sky_gradient_top_env_diffuse, 0x1E0);
	RML_ASSERT_OFFSET(GlobalShaderData, ambient_cube, 0x220);
	RML_ASSERT_OFFSET(GlobalShaderData, inv_viewport_wh, 0x320);
	RML_ASSERT_OFFSET(GlobalShaderData, packed_alpha_ref, 0x358);
	RML_ASSERT_OFFSET(GlobalShaderData, froxel_transform, 0x360);
	RML_ASSERT_SIZE(GlobalShaderData, 0x3D0);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
