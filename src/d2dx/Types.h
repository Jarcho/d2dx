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

#define D2DX_TMU_ADDRESS_ALIGNMENT 256
#define D2DX_TMU_MEMORY_SIZE (16 * 1024 * 1024)
#define D2DX_SIDE_TMU_MEMORY_SIZE (1 * 1024 * 1024)
#define D2DX_MAX_BATCHES_PER_FRAME 16384
#define D2DX_MAX_VERTICES_PER_FRAME (1024 * 1024)

#define D2DX_MAX_GAME_PALETTES 14
#define D2DX_WHITE_PALETTE_INDEX 14
#define D2DX_LOGO_PALETTE_INDEX 15
#define D2DX_MAX_PALETTES 16

#define D2DX_SURFACE_UI 0
#define D2DX_SURFACE_CURSOR 1
#define D2DX_SURFACE_FIRST 2

namespace d2dx
{
	static_assert(((D2DX_TMU_MEMORY_SIZE - 1) >> 8) == 0xFFFF, "TMU memory start addresses aren't 16 bit.");

	enum class ScreenMode
	{
		Windowed = 0,
		FullscreenDefault = 1,
	};

	enum class MajorGameState
	{
		Unknown = 0,
		FmvIntro = 1,
		TitleScreen = 2,
		Other = 3,
	};

	enum class PrimitiveType
	{
		Points = 0,
		Lines = 1,
		Triangles = 2,
		Count = 3
	};

	enum class AlphaBlend
	{
		Opaque = 0,
		SrcAlphaInvSrcAlpha = 1,
		Additive = 2,
		Multiplicative = 3,
		Count = 4
	};

	enum class RgbCombine
	{
		ColorMultipliedByTexture = 0,
		ConstantColor = 1,
		Count = 2
	};

	enum class AlphaCombine
	{
		One = 0,
		FromColor = 1,
		Count = 2
	};

	enum class GameVersion
	{
		Unsupported = 0,
		Lod109,
		Lod109b,
		Lod109d,
		Lod110f,
		Lod111,
		Lod111b,
		Lod112,
		Lod113c,
		Lod113d,
		Lod114c,
		Lod114d,
	};

	template<class T>
	struct OffsetT final
	{
		T x = T{ 0 };
		T y = T{ 0 };

		constexpr OffsetT(T x_, T y_) noexcept :
			x{ x_ },
			y{ y_ }
		{
		}

		template<class U>
		constexpr explicit OffsetT(const OffsetT<U>& other) noexcept:
			x(static_cast<T>(other.x)),
			y(static_cast<T>(other.y))
		{
		}

		constexpr OffsetT& operator+=(const OffsetT& rhs) noexcept
		{
			x += rhs.x;
			y += rhs.y;
			return *this;
		}

		constexpr OffsetT& operator-=(const OffsetT& rhs) noexcept
		{
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}

		constexpr OffsetT& operator*=(const OffsetT& rhs) noexcept
		{
			x *= rhs.x;
			y *= rhs.y;
			return *this;
		}

		constexpr OffsetT& operator+=(T rhs) noexcept
		{
			x += rhs;
			y += rhs;
			return *this;
		}

		constexpr OffsetT& operator-=(T rhs) noexcept
		{
			x -= rhs;
			y -= rhs;
			return *this;
		}

		constexpr OffsetT& operator*=(T rhs) noexcept
		{
			x *= rhs;
			y *= rhs;
			return *this;
		}

		constexpr OffsetT operator+(const OffsetT& rhs) const noexcept
		{
			return { x + rhs.x, y + rhs.y };
		}

		constexpr OffsetT operator-(const OffsetT& rhs) const noexcept
		{
			return { x - rhs.x, y - rhs.y };
		}

		constexpr OffsetT operator*(const OffsetT& rhs) const noexcept
		{
			return { x * rhs.x, y * rhs.y };
		}

		constexpr OffsetT operator+(T rhs) const noexcept
		{
			return { x + rhs, y + rhs };
		}

		constexpr OffsetT operator-(T rhs) const noexcept
		{
			return { x - rhs, y - rhs };
		}

		constexpr OffsetT operator*(T rhs) const noexcept
		{
			return { x * rhs, y * rhs };
		}

		constexpr bool operator==(const OffsetT& rhs) const noexcept = default;
		constexpr bool operator!=(const OffsetT& rhs) const noexcept = default;

		constexpr T Length() const noexcept
		{
			return std::hypot(x, y);
		}

		constexpr void Normalize() noexcept
		{
			NormalizeTo(T { 1.0 });
		}

		constexpr void NormalizeTo(T target) noexcept
		{
			T len = Length();
			T inv = target / len;
			*this *= len == T{ 0 } ? T{ 1 } : inv;
		}
	};

	using OffsetF = OffsetT<float>;
	using Offset = OffsetT<int32_t>;
	
	struct Size final
	{
		int32_t width = 0;
		int32_t height = 0;

		Size operator*(int32_t value) noexcept
		{
			return { width * value, height * value };
		}

		Size operator*(uint32_t value) noexcept
		{
			return { width * (int32_t)value, height * (int32_t)value };
		}

		Size operator*(float value) noexcept
		{
			return { static_cast<int32_t>(static_cast<float>(width) * value),
				static_cast<int32_t>(static_cast<float>(height) * value) };
		}

		bool operator==(const Size& rhs) const noexcept
		{
			return width == rhs.width && height == rhs.height;
		}
	};

	struct Rect final
	{
		Offset offset;
		Size size;

		Rect() noexcept :
			offset{ 0,0 },
			size{ 0,0 }
		{
		}

		Rect(int32_t x, int32_t y, int32_t w, int32_t h) noexcept :
			offset{ x,y },
			size{ w,h }
		{
		}

		bool IsValid() const noexcept
		{
			return size.width > 0 && size.height > 0;
		}

		bool operator==(const Rect& rhs) const noexcept
		{
			return offset == rhs.offset && size == rhs.size;
		}
	};
}
