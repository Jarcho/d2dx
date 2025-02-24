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
#include "GameHelper.h"

namespace d2dx
{
	class Batch final
	{
	public:
		Batch() noexcept :
			_textureStartAddress(0),
			_startVertexHigh_textureIndex(0),
			_textureHash(0),
			_textureHeight_textureWidth_alphaBlend(0),
			_vertexCount(0),
			_isChromaKeyEnabled_textureAtlas_paletteIndex(0),
			_filterMode_primitiveType_combiners(0),
			_startVertexLow(0),
			_surfaceId(D2DX_SURFACE_UI)
		{
		}

		inline int32_t GetPaletteIndex() const noexcept
		{
			return (int32_t)((_isChromaKeyEnabled_textureAtlas_paletteIndex & PALETTE_INDEX_MASK) >> PALETTE_INDEX_SHIFT);
		}

		inline void SetPaletteIndex(int32_t paletteIndex) noexcept
		{
			assert(paletteIndex >= 0 && paletteIndex < 16);
			_isChromaKeyEnabled_textureAtlas_paletteIndex &= ~PALETTE_INDEX_MASK;
			_isChromaKeyEnabled_textureAtlas_paletteIndex |= (paletteIndex << PALETTE_INDEX_SHIFT);
		}

		inline bool IsChromaKeyEnabled() const noexcept
		{
			return (_isChromaKeyEnabled_textureAtlas_paletteIndex & CHROMAKEY_MASK) != 0;
		}

		inline void SetIsChromaKeyEnabled(bool enable) noexcept
		{
			_isChromaKeyEnabled_textureAtlas_paletteIndex &= ~CHROMAKEY_MASK;
			_isChromaKeyEnabled_textureAtlas_paletteIndex |= (uint8_t)enable << CHROMAKEY_SHIFT;
		}

		inline RgbCombine GetRgbCombine() const noexcept
		{
			return (RgbCombine)((_filterMode_primitiveType_combiners & RGB_COMBINE_MASK) >> RGB_COMBINE_SHIFT);
		}

		inline void SetRgbCombine(RgbCombine rgbCombine) noexcept
		{
			assert((int32_t)rgbCombine >= 0 && (int32_t)rgbCombine < 2);
			_filterMode_primitiveType_combiners &= ~RGB_COMBINE_MASK;
			_filterMode_primitiveType_combiners |= (uint8_t)rgbCombine << RGB_COMBINE_SHIFT;
		}

		inline AlphaCombine GetAlphaCombine() const noexcept
		{
			return (AlphaCombine)((_filterMode_primitiveType_combiners & ALPHA_COMBINE_MASK) >> ALPHA_COMBINE_SHIFT);
		}

		inline void SetAlphaCombine(AlphaCombine alphaCombine) noexcept
		{
			assert((int32_t)alphaCombine >= 0 && (int32_t)alphaCombine < 2);
			_filterMode_primitiveType_combiners &= ~ALPHA_COMBINE_MASK;
			_filterMode_primitiveType_combiners |= (uint8_t)alphaCombine << ALPHA_COMBINE_SHIFT;
		}

		inline int32_t GetTextureWidth() const noexcept
		{
			return 1 << (((_textureHeight_textureWidth_alphaBlend & TEXTURE_WIDTH_MASK) >> TEXTURE_WIDTH_SHIFT) + 1);
		}

		inline int32_t GetTextureHeight() const noexcept
		{
			return 1 << (((_textureHeight_textureWidth_alphaBlend & TEXTURE_HEIGHT_MASK) >> TEXTURE_HEIGHT_SHIFT) + 1);
		}

		inline void SetTextureSize(int32_t width, int32_t height) noexcept
		{
			DWORD w, h;
			BitScanReverse(&w, (uint32_t)width);
			BitScanReverse(&h, (uint32_t)height);
			assert(w >= 3 && w <= 8);
			assert(h >= 3 && h <= 8);
			_textureHeight_textureWidth_alphaBlend &= ~(TEXTURE_WIDTH_MASK | TEXTURE_HEIGHT_MASK);
			_textureHeight_textureWidth_alphaBlend |= (h - 1) << TEXTURE_HEIGHT_SHIFT;
			_textureHeight_textureWidth_alphaBlend |= (w - 1) << TEXTURE_WIDTH_SHIFT;
		}

		inline AlphaBlend GetAlphaBlend() const noexcept
		{
			return (AlphaBlend)((_textureHeight_textureWidth_alphaBlend & ALPHA_BLEND_MASK) >> ALPHA_BLEND_SHIFT);
		}

		inline void SetAlphaBlend(AlphaBlend alphaBlend) noexcept
		{
			assert((uint32_t)alphaBlend < 4);
			_textureHeight_textureWidth_alphaBlend &= ~ALPHA_BLEND_MASK;
			_textureHeight_textureWidth_alphaBlend |= (uint8_t)alphaBlend << ALPHA_BLEND_SHIFT;
		}

		inline int32_t GetStartVertex() const noexcept
		{
			return _startVertexLow | ((_startVertexHigh_textureIndex & HIGH_VERTEX_MASK) >> HIGH_VERTEX_SHIFT << 16);
		}

		inline void SetStartVertex(int32_t startVertex) noexcept
		{
			assert(startVertex <= 0xFFFFF);
			_startVertexLow = startVertex & 0xFFFF;
			_startVertexHigh_textureIndex &= ~HIGH_VERTEX_MASK;
			_startVertexHigh_textureIndex |= (startVertex >> 16) << HIGH_VERTEX_SHIFT;
		}

		inline uint32_t GetVertexCount() const noexcept
		{
			return _vertexCount;
		}

		inline void SetVertexCount(uint32_t vertexCount) noexcept
		{
			assert(vertexCount >= 0 && vertexCount <= 0xFFFF);
			_vertexCount = vertexCount;
		}

		inline uint32_t SelectColorAndAlpha(uint32_t iteratedColor, uint32_t constantColor) const noexcept
		{
			const auto rgbCombine = GetRgbCombine();
			uint32_t result = (rgbCombine == RgbCombine::ConstantColor ? constantColor : iteratedColor) & 0x00FFFFFF;
			result |= constantColor & 0xFF000000;

			if (GetAlphaBlend() != AlphaBlend::SrcAlphaInvSrcAlpha)
			{
				result |= 0xFF000000;
			}

			return result;
		}

		inline uint64_t GetHash() const noexcept
		{
			return _textureHash;
		}

		void SetTextureHash(uint64_t textureHash) noexcept
		{
			_textureHash = textureHash;
		}

		inline uint32_t GetTextureAtlas() const noexcept
		{
			return (uint32_t)((_isChromaKeyEnabled_textureAtlas_paletteIndex >> 4) & 0b111);
		}

		inline void SetTextureAtlas(uint32_t textureAtlas) noexcept
		{
			assert(textureAtlas < 8);
			_isChromaKeyEnabled_textureAtlas_paletteIndex &= ~TEXTURE_ATLAS_MASK;
			_isChromaKeyEnabled_textureAtlas_paletteIndex |= (uint8_t)textureAtlas << TEXTURE_ATLAS_SHIFT;
		}

		inline uint32_t GetTextureIndex() const noexcept
		{
			return (uint32_t)(_startVertexHigh_textureIndex & TEXTURE_INDEX_MASK) >> TEXTURE_INDEX_SHIFT;
		}

		inline void SetTextureIndex(uint32_t textureIndex) noexcept
		{
			assert(textureIndex < 4096);
			_startVertexHigh_textureIndex &= ~TEXTURE_INDEX_MASK;
			_startVertexHigh_textureIndex |= (uint16_t)textureIndex << TEXTURE_INDEX_SHIFT;
		}

		inline int32_t GetTextureStartAddress() const noexcept
		{
			const uint32_t startAddress = _textureStartAddress;
			return (startAddress - 1) << 8;
		}

		inline void SetTextureStartAddress(int32_t startAddress) noexcept
		{
			assert(!(startAddress & (D2DX_TMU_ADDRESS_ALIGNMENT-1)));
			assert(startAddress >= 0 && startAddress <= (D2DX_TMU_MEMORY_SIZE - D2DX_TMU_ADDRESS_ALIGNMENT));

			startAddress >>= 8;
			++startAddress;

			assert((startAddress & 0xFFFF) != 0);

			_textureStartAddress = startAddress & 0xFFFF;
		}

		inline void SetFilterMode(GrTextureFilterMode_t mode) noexcept
		{
			assert(mode == 0 || mode == 1);
			_filterMode_primitiveType_combiners &= ~FILTER_MODE_MASK;
			_filterMode_primitiveType_combiners |= (uint8_t)mode << FILTER_MODE_SHIFT;
		}

		inline GrTextureFilterMode_t GetFilterMode() const noexcept
		{
			return (GrTextureFilterMode_t)((_filterMode_primitiveType_combiners & FILTER_MODE_MASK) >> FILTER_MODE_SHIFT);
		}

		void SetSurfaceId(int16_t id) noexcept
		{
			_surfaceId = id;
		}

		int16_t GetSurfaceId() const noexcept
		{
			return _surfaceId;
		}

		inline bool IsValid() const noexcept
		{
			return _textureStartAddress != 0;
		}

	private:
		uint64_t _textureHash;
		uint16_t _surfaceId;
		uint16_t _startVertexLow;
		uint16_t _vertexCount;
		uint16_t _textureStartAddress;							// byte address / D2DX_TMU_ADDRESS_ALIGNMENT
		uint16_t _startVertexHigh_textureIndex;					// VVVVAAAA AAAAAAAA
		uint8_t _textureHeight_textureWidth_alphaBlend;			// HHHWWWBB
		uint8_t _isChromaKeyEnabled_textureAtlas_paletteIndex;	// CAAAPPPP
		uint8_t _filterMode_primitiveType_combiners;			// ...MPPCC

		static const int TEXTURE_INDEX_SHIFT = 0;
		static const int HIGH_VERTEX_SHIFT = 12;
		static const int TEXTURE_HEIGHT_SHIFT = 5;
		static const int TEXTURE_WIDTH_SHIFT = 2;
		static const int ALPHA_BLEND_SHIFT = 0;
		static const int CHROMAKEY_SHIFT = 7;
		static const int TEXTURE_ATLAS_SHIFT = 4;
		static const int PALETTE_INDEX_SHIFT = 0;
		static const int FILTER_MODE_SHIFT = 4;
		static const int PRIMITIVE_TYPE_SHIFT = 2;
		static const int ALPHA_COMBINE_SHIFT = 1;
		static const int RGB_COMBINE_SHIFT = 0;

		static const uint16_t TEXTURE_INDEX_MASK = 0b00001111'11111111;
		static const uint16_t HIGH_VERTEX_MASK   = 0b11110000'00000000;
		static const uint8_t TEXTURE_HEIGHT_MASK = 0b11100000;
		static const uint8_t TEXTURE_WIDTH_MASK  = 0b00011100;
		static const uint8_t ALPHA_BLEND_MASK    = 0b00000011;
		static const uint8_t CHROMAKEY_MASK      = 0b10000000;
		static const uint8_t TEXTURE_ATLAS_MASK  = 0b01110000;
		static const uint8_t PALETTE_INDEX_MASK  = 0b00001111;
		static const uint8_t FILTER_MODE_MASK    = 0b00010000;
		static const uint8_t PRIMITIVE_TYPE_MASK = 0b00001100;
		static const uint8_t ALPHA_COMBINE_MASK  = 0b00000010;
		static const uint8_t RGB_COMBINE_MASK  = 0b00000001;
	};

	static_assert(sizeof(Batch) == 24, "sizeof(Batch)");
}
