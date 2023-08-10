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

namespace d2dx
{
	namespace detail
	{
		__declspec(noinline) void ThrowFromHRESULT(HRESULT hr, _In_z_ const char* func, int32_t line);
		__declspec(noinline) char* GetMessageForHRESULT(HRESULT hr, _In_z_ const char* func, int32_t line) noexcept;
		[[noreturn]] __declspec(noinline) void FatalException() noexcept;
		[[noreturn]] __declspec(noinline) void FatalError(_In_z_ const char* msg) noexcept;
	}

	struct ComException final : public std::runtime_error
	{
		ComException(HRESULT hr_, const char* func_, int32_t line_) :
			std::runtime_error(detail::GetMessageForHRESULT(hr_, func_, line_)),
			hr{ hr_ },
			func{ func_ },
			line{ line_ }
		{
		}

		HRESULT hr;
		const char* func;
		int32_t line;
	};

#define D2DX_THROW_HR(hr) detail::ThrowFromHRESULT(hr, __FUNCTION__, __LINE__)
#define D2DX_CHECK_HR(expr) { HRESULT hr = (expr); if (FAILED(hr)) { detail::ThrowFromHRESULT((hr), __FUNCTION__, __LINE__); } }
#define D2DX_FATAL_EXCEPTION detail::FatalException()
#define D2DX_FATAL_ERROR(msg) detail::FatalError(msg)
}
