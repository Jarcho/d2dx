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
#include "pch.h"
#include "Batch.h"
#include "D2DXContextFactory.h"
#include "RenderContext.h"
#include "Metrics.h"
#include "TextureCache.h"
#include "Vertex.h"
#include "Utils.h"
#include "Profiler.h"

#define MAX_FRAME_LATENCY 1
#undef ALLOW_SET_SOURCE_SIZE

using namespace d2dx;
using namespace std;

extern int (WINAPI* ShowCursor_Real)(
	_In_ BOOL bShow);

extern BOOL(WINAPI* SetWindowPos_Real)(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags);

static LRESULT CALLBACK d2dxSubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

_Use_decl_annotations_
RenderContext::RenderContext(
	HWND hWnd,
	Size gameSize,
	Size windowSize,
	ScreenMode initialScreenMode,
	ID2DXContext* d2dxContext)
{
	HRESULT hr = S_OK;

	_screenMode = initialScreenMode;

	_prevTimeStamp = TimeStamp();

	_hWnd = hWnd;
	_d2dxContext = d2dxContext;

	memset(&_shadowState, 0, sizeof(_shadowState));

	_constants.sharpness = _d2dxContext->GetOptions().GetBilinearSharpness();

	
	Offset startPos = _d2dxContext->GetOptions().GetWindowPosition();
	RECT windowRect;
	GetWindowRect(_hWnd, &windowRect);
	Offset windowPos = {
		(windowRect.right + windowRect.left) / 2,
		(windowRect.bottom - windowRect.top) / 2
	};
	if (startPos.x != -1 || startPos.y != -1)
	{
		Offset offset = startPos - windowPos;
		windowPos = startPos;
		SetWindowPos_Real(
			hWnd,
			HWND_TOP,
			windowRect.left + offset.x,
			windowRect.top + offset.y,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
	}
	_windowPos = windowPos;

	OnWinPosChanged();

	_gameSize = gameSize;
	_windowSize = windowSize;
	_renderRect = Metrics::GetRenderRect(
		gameSize,
		_screenMode == ScreenMode::FullscreenDefault ? MonitorSize()  : _windowSize,
		!_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoKeepAspectRatio));

#ifndef NDEBUG
	while (ShowCursor_Real(TRUE) < 0) {}
#endif

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	SetWindowSubclass((HWND)hWnd, d2dxSubclassWndProc, 1234, (DWORD_PTR)this);
	_isActiveWindow = GetForegroundWindow() == hWnd;

	const int32_t widthFromClientRect = clientRect.right - clientRect.left;
	const int32_t heightFromClientRect = clientRect.bottom - clientRect.top;

	_featureLevel = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL requestedFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1
	};

	_dxgiAllowTearingFlagSupported = IsAllowTearingFlagSupported();
	_frameLatencyWaitableObjectSupported = false; //  IsFrameLatencyWaitableObjectSupported();

	_swapChainCreateFlags = 0;

	if (_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoVSync))
	{
		if (_dxgiAllowTearingFlagSupported && GetWindowsVersion().major >= 10)
		{
			_swapChainCreateFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
			if (!_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoFrameTearing))
			{
				_syncStrategy = RenderContextSyncStrategy::AllowTearing;
				D2DX_LOG("Using 'AllowTearing' sync strategy.")
			}
			else
			{
				_syncStrategy = RenderContextSyncStrategy::Interval0;
				D2DX_LOG("Using 'Interval0' sync strategy.")
			}
		}
		else
		{
			_syncStrategy = RenderContextSyncStrategy::Interval0;
			D2DX_LOG("Using 'Interval0' sync strategy.")
		}
	}
	else
	{
		if (_frameLatencyWaitableObjectSupported)
		{
			_syncStrategy = RenderContextSyncStrategy::FrameLatencyWaitableObject;
			_swapChainCreateFlags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			D2DX_LOG("Using 'FrameLatencyWaitableObject' sync strategy.")
		}
		else
		{
			_syncStrategy = RenderContextSyncStrategy::Interval1;
			D2DX_LOG("Using 'Interval1' sync strategy.")
		}
	}

	if (GetWindowsVersion().major >= 10)
	{
		_swapStrategy = RenderContextSwapStrategy::FlipDiscard;
		D2DX_LOG("Using 'FlipDiscard' swap strategy.")
	}
	else
	{
		_swapStrategy = RenderContextSwapStrategy::Discard;
		D2DX_LOG("Using 'Discard' swap strategy.")
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
	swapChainDesc.SwapEffect = _swapStrategy == RenderContextSwapStrategy::FlipDiscard ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = _swapChainCreateFlags;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;

#ifdef NDEBUG
	uint32_t flags = 0;
#else
	uint32_t flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

	ComPtr<IDXGISwapChain> swapChain;

	D2DX_CHECK_HR(
		D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			flags,
			requestedFeatureLevels,
			ARRAYSIZE(requestedFeatureLevels),
			D3D11_SDK_VERSION,
			&swapChainDesc,
			swapChain.GetAddressOf(),
			&_device,
			&_featureLevel,
			&_deviceContext));

	D2DX_LOG("Created device supports %s.",
		_featureLevel == D3D_FEATURE_LEVEL_11_1 ? "D3D_FEATURE_LEVEL_11_1" :
		_featureLevel == D3D_FEATURE_LEVEL_11_0 ? "D3D_FEATURE_LEVEL_11_0" :
		_featureLevel == D3D_FEATURE_LEVEL_10_1 ? "D3D_FEATURE_LEVEL_10_1" : "unknown");

	D2DX_CHECK_HR(
		swapChain.As(&_swapChain1));

	ComPtr<IDXGIFactory> dxgiFactory;
	_swapChain1->GetParent(IID_PPV_ARGS(&dxgiFactory));
	if (dxgiFactory)
	{
		dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_WINDOW_CHANGES);
	}

	_swapChain1.As(&_swapChain2);

	if (SUCCEEDED(_deviceContext->QueryInterface(IID_PPV_ARGS(&_deviceContext1))))
	{
		D2DX_LOG("Device context supports ID3D11DeviceContext1. Will use this to discard resources and views.");
	}
	else
	{
		D2DX_LOG("Device context does not support ID3D11DeviceContext1.");
	}

	if (_syncStrategy == RenderContextSyncStrategy::FrameLatencyWaitableObject)
	{
		assert(_swapChain2);
		if (_swapChain2)
		{
			D2DX_LOG("Setting maximum frame latency to %i.", MAX_FRAME_LATENCY);
			D2DX_CHECK_HR(
				_swapChain2->SetMaximumFrameLatency(MAX_FRAME_LATENCY));
		}
	}
	else
	{
		ComPtr<IDXGIDevice1> dxgiDevice1;
		if (SUCCEEDED(_swapChain1->GetDevice(IID_PPV_ARGS(&dxgiDevice1))))
		{
			D2DX_LOG("Setting maximum frame latency to %i.", MAX_FRAME_LATENCY);
			D2DX_CHECK_HR(
				dxgiDevice1->SetMaximumFrameLatency(MAX_FRAME_LATENCY));
		}
	}

	// get a pointer directly to the back buffer
	ComPtr<ID3D11Texture2D> backbuffer;
	D2DX_CHECK_HR(
		_swapChain1->GetBuffer(0, __uuidof(ID3D11Texture2D), &backbuffer));

	// create a render target pointing to the back buffer
	D2DX_CHECK_HR(
		_device->CreateRenderTargetView(backbuffer.Get(), nullptr, &_backbufferRtv));

	backbuffer = nullptr;

	float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	_deviceContext->ClearRenderTargetView(_backbufferRtv.Get(), color);

	_vbCapacity = 1024 * 1024;

	_gameSize = { 0, 0 };
	SetSizes(_gameSize, _windowSize, _screenMode);

	Size framebufferSize = _d2dxContext->GetOptions().GetUpscaleMethod() == UpscaleMethod::Rasterize
		? _renderRect.size
		: gameSize;
	_resources = std::make_unique<RenderContextResources>(
			_vbCapacity * sizeof(Vertex),
			16 * sizeof(Constants),
		framebufferSize,
			_device.Get());

	SetRasterizerState(_resources->GetRasterizerState(true));
	_deviceContext->IASetInputLayout(_resources->GetInputLayout());

	ID3D11Buffer* cb = _resources->GetConstantBuffer();
	_deviceContext->VSSetConstantBuffers(0, 1, &cb);
	_deviceContext->PSSetConstantBuffers(0, 1, &cb);

	ID3D11SamplerState* samplerState[2] =
	{
		_resources->GetSamplerState(RenderContextSamplerState::Point),
		_resources->GetSamplerState(RenderContextSamplerState::Bilinear),
	};

	_deviceContext->PSSetSamplers(0, 2, samplerState);

	SetRenderTargets(
		_resources->GetFramebufferRtv(RenderContextFramebuffer::Game),
		_resources->GetFramebufferRtv(RenderContextFramebuffer::SurfaceId));

	uint32_t stride = sizeof(Vertex);
	uint32_t offset = 0;
	ID3D11Buffer* vbs[1] = { _resources->GetVertexBuffer() };
	_deviceContext->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
}

HWND RenderContext::GetHWnd() const
{
	return _hWnd;
}

_Use_decl_annotations_
void RenderContext::Draw(
	const Batch& batch,
	uint32_t startVertexLocation)
{
	SetBlendState(batch.GetAlphaBlend());

	ITextureCache* atlas = GetTextureCache(batch);

	RenderContextPixelShader shader = batch.GetFilterMode() == GR_TEXTUREFILTER_BILINEAR
		? RenderContextPixelShader::GameBilinear
		: RenderContextPixelShader::Game;

	SetShaderState(
		_resources->GetVertexShader(RenderContextVertexShader::Game),
		_resources->GetPixelShader(shader),
		atlas ? atlas->GetSrv(batch.GetTextureAtlas()) : nullptr,
		_resources->GetTexture1DSrv(RenderContextTexture1D::Palette));

	_deviceContext->Draw(batch.GetVertexCount(), startVertexLocation + batch.GetStartVertex());
}

bool RenderContext::IsIntegerScale() const
{
	float scaleX = ((float)_renderRect.size.width / _gameSize.width);
	float scaleY = ((float)_renderRect.size.height / _gameSize.height);

	return fabs(scaleX - floor(scaleX)) < 0.01 && fabs(scaleY - floor(scaleY)) < 0.01;
}

void RenderContext::Present()
{
	_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	float color[] = { .0f, .0f, .0f, .0f };

	SetBlendState(AlphaBlend::Opaque);

	bool needsUpscale = NeedsPostRenderUpscale();
	bool needsAA = !_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoAntiAliasing);
	ID3D11ShaderResourceView* source = _resources->GetFramebufferSrv(RenderContextFramebuffer::Game);
	ID3D11ShaderResourceView* additionalSource = _resources->GetTexture1DSrv(RenderContextTexture1D::GammaTable);
	RenderContextPixelShader pixelShader = RenderContextPixelShader::Gamma;

	// Render gamma to texture if needed
	if (needsUpscale || needsAA)
	{
		ID3D11RenderTargetView* target = _resources->GetFramebufferRtv(RenderContextFramebuffer::GammaCorrected);
		SetRenderTargets(target, nullptr);
		_deviceContext->ClearRenderTargetView(target, color);
		UpdateViewport({ 0,0,_gameSize.width, _gameSize.height });
		SetShaderState(
			_resources->GetVertexShader(RenderContextVertexShader::Display),
			_resources->GetPixelShader(RenderContextPixelShader::Gamma),
			source,
			additionalSource);
		auto startVertexLocation = _vbWriteIndex;
		uint32_t vertexCount = UpdateVerticesWithFullScreenTriangle(
			_gameSize,
			_resources->GetFramebufferSize(),
			{ 0,0,_gameSize.width, _gameSize.height });
		_deviceContext->Draw(vertexCount, startVertexLocation);
		source = _resources->GetFramebufferSrv(RenderContextFramebuffer::GammaCorrected);
	}

	// render aa to texture if needed
	if (needsUpscale && needsAA)
	{
		ID3D11RenderTargetView* target = _resources->GetFramebufferRtv(RenderContextFramebuffer::Game);
		SetShaderState(
			nullptr,
			nullptr,
			nullptr,
			nullptr);
		SetRenderTargets(target, nullptr);
		_deviceContext->ClearRenderTargetView(target, color);
		UpdateViewport({ 0,0,_gameSize.width, _gameSize.height });

		SetShaderState(
			_resources->GetVertexShader(RenderContextVertexShader::Display),
			_resources->GetPixelShader(RenderContextPixelShader::ResolveAA),
			source,
			_resources->GetFramebufferSrv(RenderContextFramebuffer::SurfaceId));

		auto startVertexLocation = _vbWriteIndex;
		auto vertexCount = UpdateVerticesWithFullScreenTriangle(
			_gameSize,
			_resources->GetFramebufferSize(),
			{ 0,0,_gameSize.width, _gameSize.height });

		_deviceContext->Draw(vertexCount, startVertexLocation);
		source = _resources->GetFramebufferSrv(RenderContextFramebuffer::Game);
	}

	if (needsUpscale)
	{
		switch (_d2dxContext->GetOptions().GetUpscaleMethod())
		{
		default:
		case UpscaleMethod::HighQuality:
			pixelShader = IsIntegerScale() ?
				RenderContextPixelShader::DisplayIntegerScale :
				RenderContextPixelShader::DisplayNonintegerScale;
			break;
		case UpscaleMethod::Bilinear:
			pixelShader = RenderContextPixelShader::DisplayBilinearScale;
			break;
		case UpscaleMethod::CatmullRom:
			pixelShader = RenderContextPixelShader::DisplayCatmullRomScale;
			break;
		case UpscaleMethod::Nearest:
			pixelShader = RenderContextPixelShader::DisplayCatmullRomScale;
			break;
		}
		additionalSource = nullptr;
	}
	else if (needsAA)
	{
		pixelShader = RenderContextPixelShader::ResolveAA;
		additionalSource = _resources->GetFramebufferSrv(RenderContextFramebuffer::SurfaceId);
	}

	SetRasterizerState(_resources->GetRasterizerState(false));
	SetRenderTargets(_backbufferRtv.Get(), nullptr);
	_deviceContext->ClearRenderTargetView(_backbufferRtv.Get(), color);
	UpdateViewport(_renderRect);

	SetShaderState(
		_resources->GetVertexShader(RenderContextVertexShader::Display),
		_resources->GetPixelShader(pixelShader),
		source,
		additionalSource);

	auto startVertexLocation = _vbWriteIndex;
	auto vertexCount = UpdateVerticesWithFullScreenTriangle(
		_gameSize,
		_resources->GetFramebufferSize(),
		_renderRect);

	_deviceContext->Draw(vertexCount, startVertexLocation);

	SetShaderState(
		nullptr,
		nullptr,
		nullptr,
		nullptr);

	if (!(_frameCount & 255))
	{
		D2DX_DEBUG_LOG("Texture cache use: %u, %u, %u, %u, %u, %u, %u",
			this->_resources->GetTextureCache(8, 8)->GetUsedCount(),
			this->_resources->GetTextureCache(16, 16)->GetUsedCount(),
			this->_resources->GetTextureCache(32, 32)->GetUsedCount(),
			this->_resources->GetTextureCache(64, 64)->GetUsedCount(),
			this->_resources->GetTextureCache(128, 128)->GetUsedCount(),
			this->_resources->GetTextureCache(256, 256)->GetUsedCount(),
			this->_resources->GetTextureCache(256, 128)->GetUsedCount());
	}

	{
		HaltSleepProfile _halt;
		Timer _timer(ProfCategory::Present);
		switch (_syncStrategy)
		{
		case RenderContextSyncStrategy::AllowTearing:
			D2DX_CHECK_HR(_swapChain1->Present(0, DXGI_PRESENT_ALLOW_TEARING));
			break;
		case RenderContextSyncStrategy::Interval0:
			D2DX_CHECK_HR(_swapChain1->Present(0, 0));
			break;
		case RenderContextSyncStrategy::FrameLatencyWaitableObject:
			D2DX_CHECK_HR(_swapChain1->Present(0, 0));
			::WaitForSingleObjectEx(_frameLatencyWaitableObject.Get(), 1000, true);
			break;
		case RenderContextSyncStrategy::Interval1:
			D2DX_CHECK_HR(_swapChain1->Present(1, 0));
			break;
		}
	}

	WriteProfile();

	auto curTimeStamp = TimeStamp();
	_frameTimeMs = TimeToMs(curTimeStamp - _prevTimeStamp);
	_prevTimeStamp = curTimeStamp;

	if (_deviceContext1)
	{
		_deviceContext1->DiscardView(_resources->GetFramebufferRtv(RenderContextFramebuffer::Game));
		_deviceContext1->DiscardView(_backbufferRtv.Get());
	}

	_resources->OnNewFrame();

	SetRenderTargets(
		_resources->GetFramebufferRtv(RenderContextFramebuffer::Game),
		_resources->GetFramebufferRtv(RenderContextFramebuffer::SurfaceId)
	);

	_deviceContext->ClearRenderTargetView(_resources->GetFramebufferRtv(RenderContextFramebuffer::Game), color);
	_deviceContext->ClearRenderTargetView(_resources->GetFramebufferRtv(RenderContextFramebuffer::SurfaceId), color);

	UpdateViewport({ 0,0,_gameSize.width, _gameSize.height });

	SetRasterizerState(_resources->GetRasterizerState(true));

	SetShaderState(
		_resources->GetVertexShader(RenderContextVertexShader::Game),
		_resources->GetPixelShader(RenderContextPixelShader::Game),
		nullptr,
		nullptr);

	++_frameCount;
}

_Use_decl_annotations_
void RenderContext::LoadGammaTable(
	_In_reads_(valueCount) const uint32_t* values,
	_In_ uint32_t valueCount)
{
	_deviceContext->UpdateSubresource(
		_resources->GetTexture1D(RenderContextTexture1D::GammaTable), 0, nullptr, values, valueCount * sizeof(uint32_t), 0);
}

void CopyImageBuf(
	uint8_t* dst,
	const uint8_t* src,
	int32_t skipHeight,
	int32_t width,
	int32_t height,
	bool is16Bit
) {
	if (is16Bit) {
		auto p = (const uint16_t*)src + width * skipHeight;
		auto end = p + width * height;
		for (; p < end; ++p, dst += 4)
		{
			uint32_t b = *p & 0b11111;
			dst[0] = (b << 3) | ((b >> 2) & 0b111);

			uint32_t g = (*p >> 5) & 0b111111;
			dst[1] = (g << 2) | ((g >> 4) & 0b11);

			uint32_t r = (*p >> 11) & 0b11111;
			dst[2] = (r << 3) | ((r >> 2) & 0b111);
		}
	}
	else
	{
		memcpy(dst, src + width * skipHeight * 4, width * height * 4);
	}
}

_Use_decl_annotations_
void RenderContext::WriteToScreen(
	const uint8_t* pixels,
	int32_t width,
	int32_t height,
	bool is16Bit,
	bool forCinematic)
{
	D3D11_MAPPED_SUBRESOURCE ms;
	SetBlendState(AlphaBlend::Opaque);
	uint32_t startVertexLocation = _vbWriteIndex;
	uint32_t vertexCount = 0;

	ID3D11PixelShader* shader = nullptr;
	if (is16Bit)
	{
		shader = _resources->GetPixelShader(RenderContextPixelShader::VideoGamma);
	}
	else
	{
		shader = _resources->GetPixelShader(RenderContextPixelShader::Video);
	}

	if (forCinematic)
	{
		SetSizes({ width, 292 }, _windowSize, _screenMode);
		D2DX_CHECK_HR(_deviceContext->Map(_resources->GetCinematicTexture(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms));
		CopyImageBuf((uint8_t*)ms.pData, pixels, 94, 640, 292, is16Bit);
		_deviceContext->Unmap(_resources->GetCinematicTexture(), 0);
		SetShaderState(
			_resources->GetVertexShader(RenderContextVertexShader::Display),
			shader,
			_resources->GetCinematicSrv(),
			nullptr);
		vertexCount = UpdateVerticesWithFullScreenTriangle(_gameSize, _resources->GetCinematicTextureSize(), { 0,0,_gameSize.width, _gameSize.height });
	}
	else
	{
		SetSizes({ width, height }, _windowSize, _screenMode);
		D2DX_CHECK_HR(_deviceContext->Map(_resources->GetVideoTexture(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms));
		CopyImageBuf((uint8_t*)ms.pData, pixels, 0, 640, 480, is16Bit);
		_deviceContext->Unmap(_resources->GetVideoTexture(), 0);
		SetShaderState(
			_resources->GetVertexShader(RenderContextVertexShader::Display),
			shader,
			_resources->GetVideoSrv(),
			nullptr);
		vertexCount = UpdateVerticesWithFullScreenTriangle(_gameSize, _resources->GetVideoTextureSize(), { 0,0,_gameSize.width, _gameSize.height });
	}

	_deviceContext->Draw(vertexCount, startVertexLocation);

	Present();
}

_Use_decl_annotations_
void RenderContext::SetBlendState(
	AlphaBlend alphaBlend)
{
	SetBlendState(_resources->GetBlendState(alphaBlend));
}

_Use_decl_annotations_
uint32_t RenderContext::BulkWriteVertices(
	const Vertex* vertices,
	uint32_t vertexCount)
{
	auto mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if ((_vbWriteIndex + vertexCount) > _vbCapacity)
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		_vbWriteIndex = 0;
		assert(vertexCount <= _vbCapacity);
		vertexCount = min(vertexCount, _vbCapacity);
	}

	const uint32_t startVertexLocation = _vbWriteIndex;

	if (vertexCount > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubResource = { 0 };
		D2DX_CHECK_HR(_deviceContext->Map(_resources->GetVertexBuffer(), 0, mapType, 0, &mappedSubResource));
		Vertex* pMappedVertices = (Vertex*)mappedSubResource.pData + _vbWriteIndex;
		memcpy(pMappedVertices, vertices, sizeof(Vertex) * vertexCount);
		_deviceContext->Unmap(_resources->GetVertexBuffer(), 0);
	}

	_vbWriteIndex += vertexCount;

	return startVertexLocation;
}

_Use_decl_annotations_
uint32_t RenderContext::UpdateVerticesWithFullScreenTriangle(
	Size srcSize,
	Size srcTextureSize,
	Rect dstRect)
{
	Vertex vertices[3] = {
		Vertex{ 0, 0, srcTextureSize.width, srcTextureSize.height, 0xFFFFFFFF, false, srcSize.height, 0, srcSize.width },
		Vertex{ static_cast<float>(dstRect.size.width * 2), 0, srcTextureSize.width, srcTextureSize.height, 0xFFFFFFFF, false, srcSize.height, 0, srcSize.width },
		Vertex{ 0, static_cast<float>(dstRect.size.height * 2), srcTextureSize.width, srcTextureSize.height, 0xFFFFFFFF, false, srcSize.height, 0, srcSize.width },
	};

	auto mapType = D3D11_MAP_WRITE_NO_OVERWRITE;

	if ((_vbWriteIndex + ARRAYSIZE(vertices)) > _vbCapacity)
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		_vbWriteIndex = 0;
	}

	D3D11_MAPPED_SUBRESOURCE mappedSubResource = { 0 };
	D2DX_CHECK_HR(_deviceContext->Map(_resources->GetVertexBuffer(), 0, mapType, 0, &mappedSubResource));
	Vertex* pMappedVertices = (Vertex*)mappedSubResource.pData + _vbWriteIndex;
	memcpy(pMappedVertices, vertices, sizeof(Vertex) * ARRAYSIZE(vertices));
	_deviceContext->Unmap(_resources->GetVertexBuffer(), 0);

	_vbWriteIndex += ARRAYSIZE(vertices);

	return ARRAYSIZE(vertices);
}

_Use_decl_annotations_
TextureCacheLocation RenderContext::UpdateTexture(
	const Batch& batch,
	const uint8_t* tmuData,
	uint32_t tmuDataSize)
{
	if (!batch.IsValid())
	{
		return { -1, -1 };
	}

	const uint64_t contentKey = batch.GetHash();

	ITextureCache* atlas = GetTextureCache(batch);

	auto tcl = atlas->FindTexture(contentKey, -1);

	if (tcl._textureAtlas < 0)
	{
		tcl = atlas->InsertTexture(contentKey, batch, tmuData, tmuDataSize);
	}

	return tcl;
}

_Use_decl_annotations_
void RenderContext::UpdateViewport(
	Rect rect)
{
	CD3D11_VIEWPORT viewport{ (float)rect.offset.x, (float)rect.offset.y, (float)rect.size.width, (float)rect.size.height };
	_deviceContext->RSSetViewports(1, &viewport);

	CD3D11_RECT scissorRect{ rect.offset.x, rect.offset.y, rect.size.width, rect.size.height };
	_deviceContext->RSSetScissorRects(1, &scissorRect);

	_constants.screenSize[0] = (float)rect.size.width;
	_constants.screenSize[1] = (float)rect.size.height;
	_constants.invScreenSize[0] = 1.0f / _constants.screenSize[0];
	_constants.invScreenSize[1] = 1.0f / _constants.screenSize[1];
	_constants.flags[0] = _d2dxContext->GetOptions().GetFlag(OptionsFlag::NoAntiAliasing) ? 0 : 1;
	_constants.flags[1] = 0;
	if (memcmp(&_constants, &_shadowState.constants, sizeof(Constants)) != 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubResource = { 0 };
		D2DX_CHECK_HR(_deviceContext->Map(_resources->GetConstantBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubResource));
		memcpy(mappedSubResource.pData, &_constants, sizeof(Constants));
		_deviceContext->Unmap(_resources->GetConstantBuffer(), 0);
		_shadowState.constants = _constants;
	}
}

_Use_decl_annotations_
void RenderContext::SetPalette(
	int32_t paletteIndex,
	const uint32_t* palette)
{
	_deviceContext->UpdateSubresource(
		_resources->GetTexture1D(RenderContextTexture1D::Palette),
		paletteIndex,
		nullptr,
		palette,
		1024,
		0);
}

const Options& RenderContext::GetOptions() const
{
	return _d2dxContext->GetOptions();
}

Options& RenderContext::GetOptions() {
	return _d2dxContext->GetOptions();
}

static LRESULT CALLBACK d2dxSubclassWndProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData)
{
	thread_local bool CURSOR_HIDDEN = false;
	thread_local bool APP_ACTIVE = true;
	thread_local bool GAME_APP_ACTIVE = true;
	RenderContext* renderContext = (RenderContext*)dwRefData;

	switch (uMsg)
	{
	case WM_ACTIVATE:
		if (wParam)
		{
			renderContext->SetActiveWindow(true);
		}
		else
		{
			renderContext->SetActiveWindow(false);
		}
		break;

	case WM_ACTIVATEAPP:
		// Don't let the game minimize/pause itself when the window isn't selected.
		if (wParam)
		{
			if (!GAME_APP_ACTIVE)
			{
				APP_ACTIVE = true;
				GAME_APP_ACTIVE = true;
				DefSubclassProc(hWnd, WM_ACTIVATEAPP, TRUE, 0);
			}
			else if (!APP_ACTIVE)
			{
				APP_ACTIVE = true;
				GAME_APP_ACTIVE = false;

				// If just ActivateApp(true) is sent, then the music will be muted for some reason.
				DefSubclassProc(hWnd, WM_ACTIVATEAPP, FALSE, 0);
				GAME_APP_ACTIVE = true;
				DefSubclassProc(hWnd, WM_ACTIVATEAPP, TRUE, 0);
			}
		}
		else
		{
			APP_ACTIVE = false;
		}
		return 0;


	case WM_SIZE:
		// Allow the game to pause/resume itself when minimized/restored.
		switch (wParam)
		{
		case SIZE_MINIMIZED:
			if (GAME_APP_ACTIVE && !APP_ACTIVE) {
				DefSubclassProc(hWnd, WM_ACTIVATEAPP, FALSE, 0);
				GAME_APP_ACTIVE = false;
			}
			break;
		
		case SIZE_RESTORED:
			if (!GAME_APP_ACTIVE && APP_ACTIVE) {
				DefSubclassProc(hWnd, WM_ACTIVATEAPP, TRUE, 0);
				GAME_APP_ACTIVE = true;
			}
			break;
		}
		break;
		
	case WM_WINDOWPOSCHANGED: 
		renderContext->ClipCursor(true);
		renderContext->OnWinPosChanged();

		// The music is muted if the game recieves this for some reason.
		return 0;

	case WM_ENTERSIZEMOVE:
		renderContext->UnclipCursor();
		break;

	case WM_SYSKEYDOWN: case WM_KEYDOWN:
		if (wParam == VK_RETURN && (HIWORD(lParam) & KF_ALTDOWN))
		{
			renderContext->ToggleFullscreen();
			return 0;
		}
		break;

	case WM_DESTROY:
		RemoveWindowSubclass(hWnd, d2dxSubclassWndProc, 1234);
		D2DXContextFactory::DestroyInstance();
		break;

	case WM_NCMOUSEMOVE:
		if (CURSOR_HIDDEN)
		{
			while (ShowCursor_Real(TRUE) < 0) {}
			CURSOR_HIDDEN = false;
		}
		return 0;

	case WM_MOUSEMOVE:
#ifdef NDEBUG
			if (!CURSOR_HIDDEN)
			{
				while (ShowCursor_Real(FALSE) >= 0) {}
				CURSOR_HIDDEN = true;
			}
#endif
			[[fallthrough]];

	default:
		if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
		{
			if (uMsg != WM_MOUSEMOVE)
			{
				renderContext->ClipCursor(false);
			}
			
			Offset mousePos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL)
			{
				ScreenToClient(hWnd, reinterpret_cast<LPPOINT>(&mousePos));
			}

			Size gameSize;
			Rect renderRect;
			renderContext->GetCurrentMetrics(&gameSize, &renderRect);

			if (mousePos.x < renderRect.offset.x || renderRect.offset.x + renderRect.size.width <= mousePos.x ||
				mousePos.y < renderRect.offset.y || renderRect.offset.y + renderRect.size.height <= mousePos.y)
			{
				// Mouse cursor is not actually on the game.
				return 0;
			}

			const OffsetF scale = {
				static_cast<float>(renderRect.size.width) / gameSize.width,
				static_cast<float>(renderRect.size.height) / gameSize.height
			};
			mousePos = Offset(OffsetF(mousePos - renderRect.offset) / scale);

			// Diablo II reacts poorly if the mouse position given is outside the game's resolution.
			mousePos.x = min(mousePos.x, gameSize.width - 1);
			mousePos.y = min(mousePos.y, gameSize.height - 1);

			lParam = ((uint32_t)mousePos.y << 16) | ((uint32_t)mousePos.x & 0xFFFF);
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

_Use_decl_annotations_
void RenderContext::SetRenderTargets(
	ID3D11RenderTargetView* rtv0,
	ID3D11RenderTargetView* rtv1)
{
	if (rtv0 != _shadowState.rtv0 ||
		rtv1 != _shadowState.rtv1)
	{
		ID3D11RenderTargetView* rtvs[2] = { rtv0, rtv1 };
		_deviceContext->OMSetRenderTargets(2, rtvs, nullptr);
		_shadowState.rtv0 = rtv0;
		_shadowState.rtv1 = rtv1;
	}
}

_Use_decl_annotations_
void RenderContext::SetRasterizerState(
	ID3D11RasterizerState* rs)
{
	if (rs != _shadowState.rs)
	{
		_deviceContext->RSSetState(rs);
		_shadowState.rs = rs;
	}
}

_Use_decl_annotations_
void RenderContext::SetBlendState(
	ID3D11BlendState* blendState)
{
	if (blendState != _shadowState.bs)
	{
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		UINT sampleMask = 0xffffffff;
		_deviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
		_shadowState.bs = blendState;
	}
}

_Use_decl_annotations_
void RenderContext::SetShaderState(
	ID3D11VertexShader* vs,
	ID3D11PixelShader* ps,
	ID3D11ShaderResourceView* srv0,
	ID3D11ShaderResourceView* srv1)
{
	if (vs != _shadowState.vs)
	{
		_deviceContext->VSSetShader(vs, NULL, 0);
		_shadowState.vs = vs;
	}

	if (ps != _shadowState.ps)
	{
		_deviceContext->PSSetShader(ps, NULL, 0);
		_shadowState.ps = ps;
	}

	if (srv0 != _shadowState.psSrv0 ||
		srv1 != _shadowState.psSrv1)
	{
		ID3D11ShaderResourceView* srvs[2] = { srv0, srv1 };
		_deviceContext->PSSetShaderResources(0, 2, srvs);
		_shadowState.psSrv0 = srv0;
		_shadowState.psSrv1 = srv1;
	}
}

_Use_decl_annotations_
ITextureCache* RenderContext::GetTextureCache(
	const Batch& batch) const
{
	return _resources->GetTextureCache(batch.GetTextureWidth(), batch.GetTextureHeight());
}

void RenderContext::ResizeBackbuffer()
{
	SetRenderTargets(nullptr, nullptr);
	_backbufferRtv = nullptr;

	D2DX_CHECK_HR(_swapChain1->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, _swapChainCreateFlags));

	ComPtr<ID3D11Texture2D> backbuffer;
	D2DX_CHECK_HR(_swapChain1->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
	D2DX_CHECK_HR(_device->CreateRenderTargetView(backbuffer.Get(), nullptr, &_backbufferRtv));
}

_Use_decl_annotations_
void RenderContext::SetSizes(
	Size gameSize,
	Size windowSize,
	ScreenMode screenMode)
{
	if (_gameSize == gameSize && _windowSize == windowSize && _screenMode == screenMode) {
		return;
	}

	bool updateGameSize = gameSize != _gameSize;
	bool updateScreenMode = screenMode != _screenMode;
	Size oldWindowSize = _windowSize;
	_gameSize = gameSize;
	_windowSize = windowSize;
	_screenMode = screenMode;
;
	Rect renderRect;
	
	if (_screenMode == ScreenMode::Windowed)
	{
		RECT oldWindowRect;
		GetWindowRect(_hWnd, &oldWindowRect);

		Offset windowPos = _windowPos;
		if (!(updateScreenMode || _useSavedWindowPos))
		{
			windowPos = {
				(oldWindowRect.left + oldWindowRect.right) / 2,
				(oldWindowRect.top + oldWindowRect.bottom) / 2
			};
			_windowPos = windowPos;
		}

		DWORD windowStyle = WS_VISIBLE;
		if (!_d2dxContext->GetOptions().GetFlag(OptionsFlag::Frameless))
		{
			windowStyle |= WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;
		}
		RECT windowRect = { 0, 0, _windowSize.width, _windowSize.height };
		AdjustWindowRect(&windowRect, windowStyle, FALSE);
		const Size monitorSize = MonitorWorkSize();
		const Size sizeLimit = {
			monitorSize.width - (windowRect.right - windowRect.left - _windowSize.width),
			monitorSize.height - (windowRect.bottom - windowRect.top - _windowSize.height)
		};

		if (_windowSize.height > sizeLimit.height)
		{
			const float aspectRatio = (float)_windowSize.width / _windowSize.height;
			_windowSize.height = sizeLimit.height;
			_windowSize.width = (int32_t)(_windowSize.height * aspectRatio);
			if (_windowSize.width > sizeLimit.width)
			{
				const float aspectRatio2 = (float)_windowSize.height / _windowSize.width;
				_windowSize.width = sizeLimit.width;
				_windowSize.height = (int32_t)(_windowSize.width * aspectRatio2);
			}

			windowRect = { 0, 0, _windowSize.width, _windowSize.height };
			AdjustWindowRect(&windowRect, windowStyle, FALSE);
		}

		renderRect = Metrics::GetRenderRect(
			_gameSize,
			_windowSize,
			!_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoKeepAspectRatio));

		const Size newSize = {
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top
		};
		Offset topLeft = {
			windowPos.x - newSize.width / 2,
			windowPos.y - newSize.height / 2
		};
		const Offset bottomRight = {
			topLeft.x + newSize.width,
			topLeft.y + newSize.height
		};
		const Offset shift = {
			min(max(_monitorWorkRect.right, oldWindowRect.right), bottomRight.x) - bottomRight.x,
			min(max(_monitorWorkRect.bottom, oldWindowRect.bottom), bottomRight.y) - bottomRight.y
		};
		topLeft.x = max(min(_monitorWorkRect.left, oldWindowRect.left), (topLeft.x + shift.x));
		topLeft.y = max(min(_monitorWorkRect.top, oldWindowRect.top), (topLeft.y + shift.y));

		if (updateScreenMode || _windowSize != oldWindowSize)
		{
			SetWindowLongPtr(_hWnd, GWL_STYLE, windowStyle);
			SetWindowPos_Real(
				_hWnd,
				HWND_TOP,
				topLeft.x,
				topLeft.y,
				newSize.width,
				newSize.height,
				SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_FRAMECHANGED);
		}
	}
	else if (_screenMode == ScreenMode::FullscreenDefault)
	{
		const Size size = MonitorSize();
		if (updateScreenMode)
		{
			RECT windowRect;
			GetWindowRect(_hWnd, &windowRect);
			_windowPos = {
				(windowRect.right + windowRect.left) / 2,
				(windowRect.bottom + windowRect.top) / 2
			};

			SetWindowLongPtr(_hWnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
			SetWindowPos_Real(
				_hWnd,
				HWND_TOP,
				_monitorRect.left,
				_monitorRect.top,
				size.width,
				size.height,
				SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_FRAMECHANGED);
		}

		renderRect = Metrics::GetRenderRect(
			_gameSize,
			size,
			!_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoKeepAspectRatio));
	}

	_useSavedWindowPos = true;
	bool updateRenderSize = renderRect.size != _renderRect.size;
	_renderRect = renderRect;
	ClipCursor(true);

	if (_resources)
	{
		if (_d2dxContext->GetOptions().GetUpscaleMethod() == UpscaleMethod::Rasterize)
		{
			if (updateRenderSize) {
				_resources->SetFramebufferSize(renderRect.size, _device.Get());
				SetRenderTargets(
					_resources->GetFramebufferRtv(RenderContextFramebuffer::Game),
					_resources->GetFramebufferRtv(RenderContextFramebuffer::SurfaceId));
			}
		}
		else if (updateGameSize)
		{
			_resources->SetFramebufferSize(gameSize, _device.Get());
			SetRenderTargets(
				_resources->GetFramebufferRtv(RenderContextFramebuffer::Game),
				_resources->GetFramebufferRtv(RenderContextFramebuffer::SurfaceId));
		}
	}

	if (!_d2dxContext->GetOptions().GetFlag(OptionsFlag::NoTitleChange))
	{
		char newWindowText[256];
		sprintf_s(newWindowText, "Diablo II DX [%ix%i, scale %i%%]",
			_gameSize.width,
			_gameSize.height,
			(int)(((float)_renderRect.size.height / _gameSize.height) * 100.0f));
		::SetWindowTextA(_hWnd, newWindowText);
	}

	D2DX_LOG("Sizes: window %ix%i, game %ix%i, render %ix%i",
		_windowSize.width,
		_windowSize.height,
		_gameSize.width,
		_gameSize.height,
		_renderRect.size.width,
		_renderRect.size.height);

	if (_resources)
	{
		ResizeBackbuffer();
		UpdateViewport({ 0,0,_gameSize.width, _gameSize.height });
		Present();
	}
}

bool RenderContext::IsFrameLatencyWaitableObjectSupported() const
{
	auto windowsVersion = GetWindowsVersion();
	return (windowsVersion.major == 6 && windowsVersion.minor >= 3) || windowsVersion.major > 6;
}

bool RenderContext::IsAllowTearingFlagSupported() const
{
	ComPtr<IDXGIFactory4> dxgiFactory4;

	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory4))))
	{
		return false;
	}

	ComPtr<IDXGIFactory5> dxgiFactory5;

	if (FAILED(dxgiFactory4.As(&dxgiFactory5)))
	{
		return false;
	}

	BOOL allowTearing = FALSE;

	if (SUCCEEDED(dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
	{
		return allowTearing;
	}

	return false;
}

void RenderContext::ToggleFullscreen()
{
	
	if (_screenMode == ScreenMode::FullscreenDefault)
	{
		SetSizes(_gameSize, _windowSize, ScreenMode::Windowed);
	}
	else
	{
		SetSizes(_gameSize, _windowSize, ScreenMode::FullscreenDefault);
	}
}

_Use_decl_annotations_
void RenderContext::GetCurrentMetrics(
	Size* gameSize,
	Rect* renderRect) const
{
	if (gameSize)
	{
		*gameSize = _gameSize;
	}

	if (renderRect)
	{
		*renderRect = _renderRect;
	}
}

void RenderContext::ClipCursor(bool resizing)
{
	if (!_isActiveWindow || (resizing != _isCursorClipped) || _d2dxContext->GetOptions().GetFlag(OptionsFlag::NoClipCursor))
	{
		return;
	}

	RECT clipRect = {
		_renderRect.offset.x,
		_renderRect.offset.y,
		_renderRect.offset.x + _renderRect.size.width,
		_renderRect.offset.y + _renderRect.size.height
	};
	::ClientToScreen(_hWnd, (LPPOINT)&clipRect.left);
    ::ClientToScreen(_hWnd, (LPPOINT)&clipRect.right);
    ::ClipCursor(&clipRect);
    _isCursorClipped = true;
}

void RenderContext::UnclipCursor()
{
	::ClipCursor(NULL);
	_isCursorClipped = false;
}

float RenderContext::GetFrameTime() const
{
	return (float)(_frameTimeMs / 1000.0);
}

int32_t RenderContext::GetFrameTimeFp() const
{
	auto frameTimeMs = (int64_t)(_frameTimeMs * (65536.0 / 1000.0));
	return (int32_t)max(INT_MIN, min(INT_MAX, frameTimeMs));
}

ScreenMode RenderContext::GetScreenMode() const
{
	return _screenMode;
}

bool RenderContext::NeedsPostRenderUpscale() const noexcept
{
	return _d2dxContext->GetOptions().GetUpscaleMethod() == UpscaleMethod::Rasterize ?
		false :
		_gameSize != _renderRect.size;
}

void RenderContext::OnWinPosChanged()
{
	_useSavedWindowPos = false;
	HMONITOR m = MonitorFromWindow(_hWnd, MONITOR_DEFAULTTONEAREST);
	if (_monitor == m) return;

	_monitor = m;
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfoW(_monitor, &info) == 0)
	{
		_monitorRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
	}
	else
	{
		_monitorRect = info.rcMonitor;
		_monitorWorkRect = info.rcWork;
	}

	Size size = MonitorSize();
	D2DX_LOG("Monitor size: %ix%i", size.width, size.height);
}
