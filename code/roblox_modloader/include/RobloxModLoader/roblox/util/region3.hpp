#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"
#include "g3d/CoordinateFrame.h"
#include "g3d/Vector3.h"

namespace RBX
{
	class Region3
	{
	public:
		G3D::CoordinateFrame cframe;
		G3D::Vector3 size;
	};

	RML_ASSERT_SIZE(Region3, 60);
}
