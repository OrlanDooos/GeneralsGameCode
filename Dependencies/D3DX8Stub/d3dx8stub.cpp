/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 OrlanDooos
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

// Stands in for d3dx8.lib in x64 builds, because D3DX8 only exists for x86.
//
// The math functions are implemented with the D3DX conventions: row vectors, which are multiplied
// from the left. The resource functions always fail, because there is no x64 d3d8.dll to create
// resources with anyway. Only the functions that the game uses are here.

#include <windows.h>
#include <d3dx8.h>

#include <math.h>
#include <stdio.h>
#include <string.h>


D3DXVECTOR4* WINAPI D3DXVec3Transform(D3DXVECTOR4 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM)
{
	const float x = pV->x, y = pV->y, z = pV->z;
	D3DXVECTOR4 out;
	out.x = x * pM->_11 + y * pM->_21 + z * pM->_31 + pM->_41;
	out.y = x * pM->_12 + y * pM->_22 + z * pM->_32 + pM->_42;
	out.z = x * pM->_13 + y * pM->_23 + z * pM->_33 + pM->_43;
	out.w = x * pM->_14 + y * pM->_24 + z * pM->_34 + pM->_44;
	*pOut = out;
	return pOut;
}

D3DXVECTOR4* WINAPI D3DXVec4Transform(D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV, CONST D3DXMATRIX *pM)
{
	const float x = pV->x, y = pV->y, z = pV->z, w = pV->w;
	D3DXVECTOR4 out;
	out.x = x * pM->_11 + y * pM->_21 + z * pM->_31 + w * pM->_41;
	out.y = x * pM->_12 + y * pM->_22 + z * pM->_32 + w * pM->_42;
	out.z = x * pM->_13 + y * pM->_23 + z * pM->_33 + w * pM->_43;
	out.w = x * pM->_14 + y * pM->_24 + z * pM->_34 + w * pM->_44;
	*pOut = out;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX *pOut, CONST D3DXMATRIX *pM)
{
	D3DXMATRIX out;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			out.m[r][c] = pM->m[c][r];
	*pOut = out;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2)
{
	// Is computed into a temporary, because pOut may alias pM1 or pM2.
	D3DXMATRIX out;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			out.m[r][c] =
				pM1->m[r][0] * pM2->m[0][c] +
				pM1->m[r][1] * pM2->m[1][c] +
				pM1->m[r][2] * pM2->m[2][c] +
				pM1->m[r][3] * pM2->m[3][c];
	*pOut = out;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM)
{
	const float* m = &pM->_11;
	float inv[16];

	inv[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8]  =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9]  = -m[0] * m[9]  * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] =  m[0] * m[9]  * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2]  =  m[1] * m[6]  * m[15] - m[1] * m[7]  * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]  - m[13] * m[3] * m[6];
	inv[6]  = -m[0] * m[6]  * m[15] + m[0] * m[7]  * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]  + m[12] * m[3] * m[6];
	inv[10] =  m[0] * m[5]  * m[15] - m[0] * m[7]  * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]  - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5]  * m[14] + m[0] * m[6]  * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]  + m[12] * m[2] * m[5];
	inv[3]  = -m[1] * m[6]  * m[11] + m[1] * m[7]  * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9]  * m[2] * m[7]  + m[9]  * m[3] * m[6];
	inv[7]  =  m[0] * m[6]  * m[11] - m[0] * m[7]  * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8]  * m[2] * m[7]  - m[8]  * m[3] * m[6];
	inv[11] = -m[0] * m[5]  * m[11] + m[0] * m[7]  * m[9]  + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]  - m[8]  * m[1] * m[7]  + m[8]  * m[3] * m[5];
	inv[15] =  m[0] * m[5]  * m[10] - m[0] * m[6]  * m[9]  - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]  + m[8]  * m[1] * m[6]  - m[8]  * m[2] * m[5];

	const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (pDeterminant)
		*pDeterminant = det;

	// Like D3DX, fails for a singular matrix and leaves pOut untouched then.
	if (det == 0.0f)
		return nullptr;

	const float invDet = 1.0f / det;
	float* out = &pOut->_11;
	for (int i = 0; i < 16; ++i)
		out[i] = inv[i] * invDet;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz)
{
	memset(pOut, 0, sizeof(D3DXMATRIX));
	pOut->_11 = sx;
	pOut->_22 = sy;
	pOut->_33 = sz;
	pOut->_44 = 1.0f;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z)
{
	memset(pOut, 0, sizeof(D3DXMATRIX));
	pOut->_11 = 1.0f;
	pOut->_22 = 1.0f;
	pOut->_33 = 1.0f;
	pOut->_44 = 1.0f;
	pOut->_41 = x;
	pOut->_42 = y;
	pOut->_43 = z;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT Angle)
{
	const float s = sinf(Angle);
	const float c = cosf(Angle);
	memset(pOut, 0, sizeof(D3DXMATRIX));
	pOut->_11 = c;
	pOut->_12 = s;
	pOut->_21 = -s;
	pOut->_22 = c;
	pOut->_33 = 1.0f;
	pOut->_44 = 1.0f;
	return pOut;
}

UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF)
{
	UINT size = 0;

	switch (FVF & D3DFVF_POSITION_MASK)
	{
	case D3DFVF_XYZ:    size += 3 * sizeof(float); break;
	case D3DFVF_XYZRHW: size += 4 * sizeof(float); break;
	case D3DFVF_XYZB1:  size += 4 * sizeof(float); break;
	case D3DFVF_XYZB2:  size += 5 * sizeof(float); break;
	case D3DFVF_XYZB3:  size += 6 * sizeof(float); break;
	case D3DFVF_XYZB4:  size += 7 * sizeof(float); break;
	case D3DFVF_XYZB5:  size += 8 * sizeof(float); break;
	default: break;
	}

	if (FVF & D3DFVF_NORMAL)   size += 3 * sizeof(float);
	if (FVF & D3DFVF_PSIZE)    size += sizeof(float);
	if (FVF & D3DFVF_DIFFUSE)  size += sizeof(DWORD);
	if (FVF & D3DFVF_SPECULAR) size += sizeof(DWORD);

	// Each texture coordinate set has a 2 bit format code, see D3DFVF_TEXCOORDSIZE1 to 4.
	const UINT texCount = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	for (UINT i = 0; i < texCount; ++i)
	{
		switch ((FVF >> (16 + i * 2)) & 0x3)
		{
		case D3DFVF_TEXTUREFORMAT1: size += 1 * sizeof(float); break;
		case D3DFVF_TEXTUREFORMAT2: size += 2 * sizeof(float); break;
		case D3DFVF_TEXTUREFORMAT3: size += 3 * sizeof(float); break;
		case D3DFVF_TEXTUREFORMAT4: size += 4 * sizeof(float); break;
		}
	}

	return size;
}

HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, LPSTR pBuffer, UINT BufferLen)
{
	if (pBuffer == nullptr || BufferLen == 0)
		return D3DERR_INVALIDCALL;
	_snprintf_s(pBuffer, BufferLen, _TRUNCATE, "HRESULT 0x%08lX (D3DX8 error strings are not available in x64 builds)", (unsigned long)hr);
	return D3D_OK;
}

HRESULT WINAPI D3DXAssembleShader(LPCVOID, UINT, DWORD, LPD3DXBUFFER* ppConstants,
	LPD3DXBUFFER* ppCompiledShader, LPD3DXBUFFER* ppCompilationErrors)
{
	if (ppConstants) *ppConstants = nullptr;
	if (ppCompiledShader) *ppCompiledShader = nullptr;
	if (ppCompilationErrors) *ppCompilationErrors = nullptr;
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE8, CONST PALETTEENTRY*, CONST RECT*,
	LPDIRECT3DSURFACE8, CONST PALETTEENTRY*, CONST RECT*, DWORD, D3DCOLOR)
{
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE8, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
	LPDIRECT3DTEXTURE8* ppTexture)
{
	if (ppTexture) *ppTexture = nullptr;
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXCreateCubeTexture(LPDIRECT3DDEVICE8, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
	LPDIRECT3DCUBETEXTURE8* ppCubeTexture)
{
	if (ppCubeTexture) *ppCubeTexture = nullptr;
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXCreateVolumeTexture(LPDIRECT3DDEVICE8, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
	LPDIRECT3DVOLUMETEXTURE8* ppVolumeTexture)
{
	if (ppVolumeTexture) *ppVolumeTexture = nullptr;
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE8, LPCSTR, UINT, UINT, UINT, DWORD, D3DFORMAT,
	D3DPOOL, DWORD, DWORD, D3DCOLOR, D3DXIMAGE_INFO*, PALETTEENTRY*, LPDIRECT3DTEXTURE8* ppTexture)
{
	if (ppTexture) *ppTexture = nullptr;
	return D3DERR_NOTAVAILABLE;
}

HRESULT WINAPI D3DXFilterTexture(LPDIRECT3DBASETEXTURE8, CONST PALETTEENTRY*, UINT, DWORD)
{
	return D3DERR_NOTAVAILABLE;
}
