#pragma once

#include "RobloxModLoader/render/commands.hpp"
#include "RobloxModLoader/render/engine_stage.hpp"
#include "RobloxModLoader/render/frame_resources.hpp"
#include "RobloxModLoader/render/injection_point.hpp"
#include "RobloxModLoader/render/passes/fullscreen_pass.hpp"
#include "RobloxModLoader/render/passes/scene_pass.hpp"
#include "RobloxModLoader/render/render_graph.hpp"
#include "RobloxModLoader/render/render_pass.hpp"
#include "RobloxModLoader/rml_export.hpp"

namespace rml::render
{
	RML_EXPORT bool enable_output_readback(bool enabled);
}
