#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"
#include "g3d/Vector3.h"

namespace RBX
{
	class Ray
	{
	public:
		G3D::Vector3 origin;
		G3D::Vector3 direction;
	};

	RML_ASSERT_SIZE(Ray, 24);
}
