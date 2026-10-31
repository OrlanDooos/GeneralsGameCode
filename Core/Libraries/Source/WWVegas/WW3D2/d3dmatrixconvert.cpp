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

// The conversions between the WWMath matrices and the D3D matrices, as declared in matrix3d.h and
// matrix4.h. They live here instead of in WWMath, so that WWMath does not depend on the DirectX 8
// headers. All their users are renderer code.

#include "WWMath/matrix3d.h"
#include "WWMath/matrix4.h"

#include "WWLib/win.h"
#include <d3d8types.h>
#include <d3dx8math.h>


void To_D3DMATRIX(_D3DMATRIX& dxm, const Matrix3D& m)
{
	dxm.m[0][0] = m[0][0];
	dxm.m[0][1] = m[1][0];
	dxm.m[0][2] = m[2][0];
	dxm.m[0][3] = 0.0f;

	dxm.m[1][0] = m[0][1];
	dxm.m[1][1] = m[1][1];
	dxm.m[1][2] = m[2][1];
	dxm.m[1][3] = 0.0f;

	dxm.m[2][0] = m[0][2];
	dxm.m[2][1] = m[1][2];
	dxm.m[2][2] = m[2][2];
	dxm.m[2][3] = 0.0f;

	dxm.m[3][0] = m[0][3];
	dxm.m[3][1] = m[1][3];
	dxm.m[3][2] = m[2][3];
	dxm.m[3][3] = 1.0f;
}

_D3DMATRIX To_D3DMATRIX(const Matrix3D& m)
{
	_D3DMATRIX dxm;
	To_D3DMATRIX(dxm, m);
	return dxm;
}

D3DXMATRIX To_D3DXMATRIX(const Matrix3D& m)
{
	D3DXMATRIX dxm;
	To_D3DMATRIX(dxm, m);
	return dxm;
}


void To_D3DMATRIX(_D3DMATRIX& dxm, const Matrix4x4& m)
{
	dxm.m[0][0] = m[0][0];
	dxm.m[0][1] = m[1][0];
	dxm.m[0][2] = m[2][0];
	dxm.m[0][3] = m[3][0];

	dxm.m[1][0] = m[0][1];
	dxm.m[1][1] = m[1][1];
	dxm.m[1][2] = m[2][1];
	dxm.m[1][3] = m[3][1];

	dxm.m[2][0] = m[0][2];
	dxm.m[2][1] = m[1][2];
	dxm.m[2][2] = m[2][2];
	dxm.m[2][3] = m[3][2];

	dxm.m[3][0] = m[0][3];
	dxm.m[3][1] = m[1][3];
	dxm.m[3][2] = m[2][3];
	dxm.m[3][3] = m[3][3];
}

_D3DMATRIX To_D3DMATRIX(const Matrix4x4& m)
{
	_D3DMATRIX dxm;
	To_D3DMATRIX(dxm, m);
	return dxm;
}

D3DXMATRIX To_D3DXMATRIX(const Matrix4x4& m)
{
	D3DXMATRIX dxm;
	To_D3DMATRIX(dxm, m);
	return dxm;
}

void To_Matrix4x4(Matrix4x4& m, const _D3DMATRIX& dxm)
{
	m[0][0] = dxm.m[0][0];
	m[0][1] = dxm.m[1][0];
	m[0][2] = dxm.m[2][0];
	m[0][3] = dxm.m[3][0];

	m[1][0] = dxm.m[0][1];
	m[1][1] = dxm.m[1][1];
	m[1][2] = dxm.m[2][1];
	m[1][3] = dxm.m[3][1];

	m[2][0] = dxm.m[0][2];
	m[2][1] = dxm.m[1][2];
	m[2][2] = dxm.m[2][2];
	m[2][3] = dxm.m[3][2];

	m[3][0] = dxm.m[0][3];
	m[3][1] = dxm.m[1][3];
	m[3][2] = dxm.m[2][3];
	m[3][3] = dxm.m[3][3];
}

Matrix4x4 To_Matrix4x4(const _D3DMATRIX& dxm)
{
	Matrix4x4 m;
	To_Matrix4x4(m, dxm);
	return m;
}
