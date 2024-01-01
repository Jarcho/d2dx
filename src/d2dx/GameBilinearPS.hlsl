/*
	This file is part of D2DX.

	Copyright (C) 2021  Bolrog

	D2DX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	D2DX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with D2DX.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "Constants.hlsli"
#include "Game.hlsli"

Texture2DArray<uint> tex : register(t0);
Texture1DArray palette : register(t1);

void main(
	in GamePSInput ps_in,
	out GamePSOutput ps_out)
{
	const uint atlasIndex = ps_in.atlasIndex_paletteIndex_surfaceId_flags.x;
	const bool chromaKeyEnabled = ps_in.atlasIndex_paletteIndex_surfaceId_flags.w & 1;
	const uint surfaceId = ps_in.atlasIndex_paletteIndex_surfaceId_flags.z;
	const uint paletteIndex = ps_in.atlasIndex_paletteIndex_surfaceId_flags.y;

	if (chromaKeyEnabled && tex.Load(int4(ps_in.tc, atlasIndex, 0)) == 0)
		discard;

	const float2 tc = ps_in.tc - 0.5;
	const int2 ulTc = int2(tc);
	const int2 lrTc = ulTc + 1;
	const uint i1 = tex.Load(int4(ulTc, atlasIndex, 0));
	const uint i2 = tex.Load(int4(lrTc.x, ulTc.y, atlasIndex, 0));
	const uint i3 = tex.Load(int4(ulTc.x, lrTc.y, atlasIndex, 0));
	const uint i4 = tex.Load(int4(lrTc, atlasIndex, 0));
	const float4 c1 = palette.Load(int3(i1, paletteIndex, 0));
	const float4 c2 = palette.Load(int3(i2, paletteIndex, 0));
	const float4 c3 = palette.Load(int3(i3, paletteIndex, 0));
	const float4 c4 = palette.Load(int3(i4, paletteIndex, 0));

	const float2 blend = saturate((tc - float2(ulTc)) * c_sharpness - ((c_sharpness - 1.0) * 0.5));

	const float4 c12 = chromaKeyEnabled && (i1 == 0 || i2 == 0)
		? (i1 == 0 ? c2 : c1)
		: lerp(c1, c2, blend.xxxx);

	const float4 c34 = chromaKeyEnabled && (i3 == 0 || i4 == 0)
		? (i3 == 0 ? c4 : c3)
		: lerp(c3, c4, blend.xxxx);

	const bool c12Discard = i1 == 0 && i2 == 0;
	const bool c34Discard = i3 == 0 && i4 == 0;
	const float4 c = chromaKeyEnabled && (c12Discard || c34Discard)
		? (c12Discard ? c34 : c12)
		: lerp(c12, c34, blend.yyyy);

    ps_out.color = ps_in.color * c;
    ps_out.surfaceId = surfaceId * 1.0 / 16383.0;
}