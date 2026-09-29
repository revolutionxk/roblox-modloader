#ifndef _70F7A2EE1B6E4dd0AF07E4BFA609A3D1
#define _70F7A2EE1B6E4dd0AF07E4BFA609A3D1

#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "g3d/AABox.h"
#include "g3d/Box.h"
#include "g3d/Color3.h"
#include "g3d/Color3uint8.h"
#include "g3d/Color4.h"
#include "g3d/Color4uint8.h"
#include "g3d/CoordinateFrame.h"
#include "g3d/Line.h"
#include "g3d/LineSegment.h"
#include "g3d/Matrix3.h"
#include "g3d/Matrix4.h"
#include "g3d/Plane.h"
#include "g3d/Rect2D.h"
#include "g3d/Sphere.h"
#include "g3d/Vector2.h"
#include "g3d/Vector2int16.h"
#include "g3d/Vector3.h"
#include "g3d/Vector3int16.h"
#include "g3d/Vector4.h"
#include "g3d/debug.h"
#include "g3d/g3dmath.h"
#include "g3d/vectorMath.h"
#include "rbxg3d/RbxCamera.h"

#if defined(__clang__)
	#pragma clang diagnostic pop
#endif

// TODO: this can cause namespace collisions:
//using G3D::Array;

#include "ray.hpp"

namespace RBX
{
	typedef G3D::Vector2 Vector2;
	typedef G3D::Vector3 Vector3;
	typedef G3D::Vector4 Vector4;
	typedef G3D::Vector2int16 Vector2int16;
	typedef G3D::Vector3int16 Vector3int16;
	typedef G3D::Color4uint8 Color4uint8;
	typedef G3D::Color3uint8 Color3uint8;
	typedef G3D::Matrix3 Matrix3;
	typedef G3D::Matrix4 Matrix4;
	typedef G3D::CoordinateFrame CoordinateFrame;
	typedef G3D::Plane Plane;
	typedef G3D::Line Line;
	typedef G3D::LineSegment LineSegment;
	typedef G3D::Color3 Color3;
	typedef G3D::Color4 Color4;
	typedef G3D::Rect2D Rect2D;
	typedef G3D::Box Box;
	typedef G3D::AABox AABox;
	typedef G3D::Sphere Sphere;

	enum IntersectResult
	{
		irNone = 0,
		irPartial = 1,
		irFull = 2
	};
}

namespace G3D
{
	std::size_t hash_value(const G3D::Vector3& v);

	std::size_t hash_value(const G3D::Vector3int16& v);
}

#endif
