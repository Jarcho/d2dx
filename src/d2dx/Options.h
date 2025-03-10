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
#pragma once

#include "Types.h"

namespace d2dx
{
	enum class OptionsFlag
	{
		NoClipCursor,
		NoResMod,
		NoFpsMod,
		NoWide,
		NoLogo,
		NoAntiAliasing,
		NoCompatModeFix,
		NoTitleChange,
		NoVSync,
		NoKeepAspectRatio,
		NoFrameTearing,

		DbgDumpTextures,

		Frameless,

		Count
	};

	enum class UpscaleMethod
	{
		HighQuality = 0,
		Bilinear = 1,
		CatmullRom = 2,
		Nearest = 3,
		Rasterize = 4,
		Count = 5
	};

	class Options final
	{
	public:
		Options();
		~Options() noexcept;

		void ApplyCfg(
			_In_z_ const char* cfg);

		void ApplyCommandLine(
			_In_z_ const char* cmdLine);

		bool GetFlag(
			_In_ OptionsFlag flag) const;

		void SetFlag(
			_In_ OptionsFlag flag,
			_In_ bool value);

		float GetWindowScale() const;

		void SetWindowScale(
			_In_ float zoomLevel);

		Offset GetWindowPosition() const;

		void SetWindowPosition(
			_In_ Offset windowPosition);

		Size GetUserSpecifiedGameSize() const;

		void SetUserSpecifiedGameSize(
			_In_ Size size);

		UpscaleMethod GetUpscaleMethod() const;

		void SetUpscaleMethod(
			_In_ UpscaleMethod upscaleMethod) noexcept;
		
		float GetBilinearSharpness() const;

		void SetBilinearSharpness(
			_In_ float sharpness) noexcept;

	private:
		uint32_t _flags = (1 << (uint32_t)OptionsFlag::NoVSync) | (1 << (uint32_t)OptionsFlag::NoFrameTearing);
		float _windowScale = 1;
		Offset _windowPosition{ -1, -1 };
		Size _userSpecifiedGameSize{ -1, -1 };
		UpscaleMethod _upscaleMethod{ UpscaleMethod::HighQuality };
		float _bilinearSharpness = 2.0;
	};
}