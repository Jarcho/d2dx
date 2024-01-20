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

#include "Batch.h"
#include "Buffer.h"
#include "BuiltinMods.h"
#include "GameHelper.h"
#include "ID2DXContext.h"
#include "IGlide3x.h"
#include "IRenderContext.h"
#include "IWin32InterceptionHandler.h"
#include "TextureHasher.h"
#include "Vertex.h"

namespace d2dx
{
	class Vertex;

	class D2DXContext final :
		public ID2DXContext
	{
	public:
		D2DXContext();
		
		virtual ~D2DXContext() noexcept;

#pragma region IGlide3x

		virtual const char* OnGetString(
			_In_ uint32_t pname);
		
		virtual uint32_t OnGet(
			_In_ uint32_t pname,
			_In_ uint32_t plength,
			_Out_writes_(plength) int32_t* params);
		
		virtual void OnSstWinOpen(
			_In_ uint32_t hWnd,
			_In_ int32_t width,
			_In_ int32_t height);
		
		virtual void OnVertexLayout(
			_In_ uint32_t param,
			_In_ int32_t offset);
		
		virtual void OnTexDownload(
			_In_ uint32_t tmu,
			_In_reads_(width * height) const uint8_t* sourceAddress,
			_In_ uint32_t startAddress,
			_In_ int32_t width,
			_In_ int32_t height);
		
		virtual void OnTexSource(
			_In_ uint32_t tmu,
			_In_ uint32_t startAddress,
			_In_ int32_t width,
			_In_ int32_t height,
			_In_ uint32_t largeLog2,
			_In_ uint32_t ratioLog2);

		virtual void OnConstantColorValue(
			_In_ uint32_t color);
		
		virtual void OnAlphaBlendFunction(
			_In_ GrAlphaBlendFnc_t rgb_sf,
			_In_ GrAlphaBlendFnc_t rgb_df,
			_In_ GrAlphaBlendFnc_t alpha_sf,
			_In_ GrAlphaBlendFnc_t alpha_df);
		
		virtual void OnColorCombine(
			_In_ GrCombineFunction_t function,
			_In_ GrCombineFactor_t factor,
			_In_ GrCombineLocal_t local,
			_In_ GrCombineOther_t other,
			_In_ bool invert);
		
		virtual void OnAlphaCombine(
			_In_ GrCombineFunction_t function,
			_In_ GrCombineFactor_t factor,
			_In_ GrCombineLocal_t local,
			_In_ GrCombineOther_t other,
			_In_ bool invert);

		virtual void OnDrawPoint(
			_In_ const void* pt,
			_In_ uint32_t gameContext);

		virtual void OnDrawLine(
			_In_ const void* v1,
			_In_ const void* v2,
			_In_ uint32_t gameContext);
		
		virtual void OnDrawVertexArray(
			_In_ uint32_t mode,
			_In_ uint32_t count,
			_In_reads_(count) uint8_t** pointers,
			_In_ uint32_t gameContext);
		
		virtual void OnDrawVertexArrayContiguous(
			_In_ uint32_t mode,
			_In_ uint32_t count,
			_In_reads_(count * stride) uint8_t* vertex,
			_In_ uint32_t stride,
			_In_ uint32_t gameContext);
		
		virtual void OnTexDownloadTable(
			_In_ GrTexTable_t type,
			_In_reads_bytes_(256 * 4) void* data);
		
		virtual void OnLoadGammaTable(
			_In_ uint32_t nentries,
			_In_reads_(nentries) uint32_t* red, 
			_In_reads_(nentries) uint32_t* green,
			_In_reads_(nentries) uint32_t* blue);
		
		virtual void OnChromakeyMode(
			_In_ GrChromakeyMode_t mode);
		
		virtual void OnLfbUnlock(
			_In_reads_bytes_(640 * 480 * 4) const uint8_t* lfbPtr,
			_In_ bool is16Bit);
		
		virtual void OnGammaCorrectionRGB(
			_In_ float red,
			_In_ float green,
			_In_ float blue);
		
		virtual void OnBufferSwap();

		virtual void OnTexFilterMode(
			_In_ GrChipID_t tmu,
			_In_ GrTextureFilterMode_t filterMode);

#pragma endregion IGlide3x

#pragma region ID2DXContext

		virtual void SetCustomResolution(
			_In_ Size size) override;
		
		virtual Size GetSuggestedCustomResolution() override;

		virtual void DisableBuiltinResMod() override;

		virtual const Options& GetOptions() const override;
		virtual Options& GetOptions() override;

		virtual uint32_t GetActiveThreadId() const noexcept override
		{
			return _threadId;
		}

#pragma endregion ID2DXContext

#pragma region IWin32InterceptionHandler
		
		virtual Offset OnSetCursorPos(
			_In_ Offset pos) override;

		virtual Offset OnMouseMoveMessage(
			_In_ Offset pos) override;

#pragma endregion IWin32InterceptionHandler

#pragma region ID2InterceptionHandler

		virtual void InterceptDrawText(
			_Inout_z_ wchar_t* str) override;

		virtual uint16_t SetSurface(
			_In_ uint16_t id) override
		{
			uint16_t prev = _scratchBatch.GetSurfaceId();
			_scratchBatch.SetSurfaceId(id);
			return prev;
		}

		virtual uint16_t SetNewSurface() override
		{
			uint16_t prev = _scratchBatch.GetSurfaceId();
			_scratchBatch.SetSurfaceId(_nextSurface++);
			return prev;
		}

#pragma endregion ID2InterceptionHandler

	private:		
		void CheckMajorGameState();

		void PrepareLogoTextureBatch();

		void InsertLogoOnTitleScreen();

		void DrawBatches(
			_In_ uint32_t startVertexLocation);

		const Batch PrepareBatchForSubmit(
			_In_ Batch batch,
			_In_ PrimitiveType primitiveType,
			_In_ uint32_t vertexCount,
			_In_ uint32_t gameContext) const;
		
		void EnsureReadVertexStateUpdated(
			_In_ const Batch& batch);

		void FillVertexSurfaceId(
			_In_ const Batch& batch);

		Offset GameToWinCursorPos(
			_In_ Offset pos);

		struct GlideState
		{
			Buffer<uint8_t> tmuMemory{ D2DX_TMU_MEMORY_SIZE };
			Buffer<uint8_t> sideTmuMemory{ D2DX_SIDE_TMU_MEMORY_SIZE };
			Buffer<uint32_t> palettes{ D2DX_MAX_PALETTES * 256 };
			Buffer<uint32_t> gammaTable{ 256 };
			uint32_t constantColor{ 0xFFFFFFFF };
			int32_t stShift{ 0 };
		};

		struct ReadVertexState
		{
			Vertex templateVertex;
			uint32_t constantColorMask{ 0 };
			uint32_t iteratedColorMask{ 0 };
			uint32_t maskedConstantColor{ 0 };
			bool isDirty{ false };
		};

		GlideState _glideState;
		ReadVertexState _readVertexState;

		Batch _scratchBatch;
		uint16_t _nextSurface = D2DX_SURFACE_FIRST;

		int32_t _frame;
		std::shared_ptr<IRenderContext> _renderContext;
		GameHelper _gameHelper;
		BuiltinMods _builtinMods;
		TextureHasher _textureHasher;

		MajorGameState _majorGameState;
		ScreenMode _initialScreenMode;

		Buffer<uint64_t> _paletteKeys;

		uint32_t _batchCount;
		Buffer<Batch> _batches;

		uint32_t _vertexCount;
		Buffer<Vertex> _vertices;

		Options _options;
		Batch _logoTextureBatch;
		
		Size _customGameSize;
		Size _suggestedGameSize;

		uint32_t _lastScreenOpenMode;

		Size _gameSize;

		Offset _playerScreenPos = { 0,0 };

		OffsetF _avgDir = { 0.0f, 0.0f };

		uint32_t _threadId = 0;
	};
}
