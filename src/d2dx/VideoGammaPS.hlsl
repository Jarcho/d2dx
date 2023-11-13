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
#include "Display.hlsli"

Texture2D tex : register(t0);

float4 main(
	in DisplayPSInput ps_in) : SV_TARGET
{
    return pow(tex.Sample(BilinearSampler, ps_in.tc), 1/1.5) * float4(1, 1, 1, 0) + float4(0, 0, 0, 1);
}
