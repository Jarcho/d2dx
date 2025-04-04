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
	struct IWin32InterceptionHandler abstract
	{
		virtual ~IWin32InterceptionHandler() noexcept {}

		virtual Offset OnSetCursorPos(
			_In_ Offset pos) = 0;

		virtual Offset OnMouseMoveMessage(
			_In_ Offset pos) = 0;

		virtual bool IsD2ClientModule(
			_In_ HMODULE module) = 0;
	};
}
