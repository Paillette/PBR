#pragma once

#include "mat4.h"

struct Vertex
{
	vec3 position;			//  3x4 octets = 12
	vec3 normal;			// +3x4 octets = 24
	vec2 texcoords;			// +2x4 octets = 32
	vec4 tangent;
	vec4 bitangent;

	static constexpr float EPSILON = 0.001f;
	static inline bool IsSame(const vec2& lhs, const vec2& rhs)
	{
		if (fabsf(lhs.x - rhs.x) < EPSILON && fabsf(lhs.y - rhs.y) < EPSILON)
			return true;
		return false;
	}
	static inline bool IsSame(const vec3& lhs, const vec3& rhs)
	{
		if (fabsf(lhs.x - rhs.x) < EPSILON && fabsf(lhs.y - rhs.y) < EPSILON && fabsf(lhs.z - rhs.z) < EPSILON)
			return true;
		return false;
	}
	inline bool IsSame(const Vertex& v) const
	{
		if (IsSame(position, v.position)
			&& IsSame(normal, v.normal)
			&& IsSame(texcoords, v.texcoords))
			return true;
		return false;
	}
};

