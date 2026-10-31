/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// BezierSegment.h ////////////////////////////////////////////////////////////////////////////////
// John K McDonald, Jr.
// September 2002
// DO NOT DISTRIBUTE

#pragma once

#include "Common/STLTypedefs.h"
#include "Lib/BaseDefines.h"

// Without deterministic math the retail compatible results come from D3DX. D3DX8 only exists for
// x86, so x64 always uses the own implementation below.
#if !USE_DETERMINISTIC_MATH && !(defined(_M_X64) || defined(__x86_64__))
#define BEZIER_USE_D3DX (1)
#include <d3dx8math.h>
#endif

#define USUAL_TOLERANCE 1.0f

// Has the memory layout of D3DXVECTOR4.
struct BezierVector4
{
	float x, y, z, w;

	BezierVector4() {}
	BezierVector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Has the memory layout of D3DXMATRIX.
struct BezierMatrix
{
	float m[4][4];

	BezierMatrix(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
		m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
		m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
	}
};

class BezierMath
{
public:
	// Row vector times matrix, like D3DXVec4Transform.
	static void Vec4Transform(BezierVector4* out, const BezierVector4* v, const BezierMatrix* m)
	{
#if BEZIER_USE_D3DX
		::D3DXVec4Transform(reinterpret_cast<D3DXVECTOR4*>(out), reinterpret_cast<const D3DXVECTOR4*>(v),
			reinterpret_cast<const D3DXMATRIX*>(m));
#else
		const float x = v->x, y = v->y, z = v->z, w = v->w;
		out->x = ((x * m->m[0][0] + y * m->m[1][0]) + z * m->m[2][0]) + w * m->m[3][0];
		out->y = ((x * m->m[0][1] + y * m->m[1][1]) + z * m->m[2][1]) + w * m->m[3][1];
		out->z = ((x * m->m[0][2] + y * m->m[1][2]) + z * m->m[2][2]) + w * m->m[3][2];
		out->w = ((x * m->m[0][3] + y * m->m[1][3]) + z * m->m[2][3]) + w * m->m[3][3];
#endif
	}

	static float Vec4Dot(const BezierVector4* a, const BezierVector4* b)
	{
#if BEZIER_USE_D3DX
		return ::D3DXVec4Dot(reinterpret_cast<const D3DXVECTOR4*>(a), reinterpret_cast<const D3DXVECTOR4*>(b));
#else
		return ((a->x * b->x + a->y * b->y) + a->z * b->z) + a->w * b->w;
#endif
	}
};

#if BEZIER_USE_D3DX
static_assert(sizeof(BezierVector4) == sizeof(D3DXVECTOR4), "BezierVector4 must match D3DXVECTOR4");
static_assert(sizeof(BezierMatrix) == sizeof(D3DXMATRIX), "BezierMatrix must match D3DXMATRIX");
#endif

class BezierSegment
{
	protected:
		static const BezierMatrix s_bezBasisMatrix;
		Coord3D m_controlPoints[4];

	public:	// Constructors
		BezierSegment();
		BezierSegment(Real x0, Real y0, Real z0,
									Real x1, Real y1, Real z1,
									Real x2, Real y2, Real z2,
									Real x3, Real y3, Real z3);

		BezierSegment(Real cp[12]);


		BezierSegment(const Coord3D& cp0,
									const Coord3D& cp1,
									const Coord3D& cp2,
									const Coord3D& cp3);

		BezierSegment(Coord3D cp[4]);

	public:
		void evaluateBezSegmentAtT(Real tValue, Coord3D *outResult) const;
		void getSegmentPoints(Int numSegments, VecCoord3D *outResult) const;
		Real getApproximateLength(Real withinTolerance = USUAL_TOLERANCE) const;

		void splitSegmentAtT(Real tValue, BezierSegment &outSeg1, BezierSegment &outSeg2) const;

	public:	// He get's friendly access.
		friend class BezFwdIterator;
};
