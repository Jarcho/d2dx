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
#include "Detours.h"
#include "Utils.h"
#include "D2DXContextFactory.h"
#include "GameHelper.h"
#include "IWin32InterceptionHandler.h"
#include "Profiler.h"

using namespace d2dx;

#pragma comment(lib, "../../thirdparty/detours/detours.lib")

static bool hasDetoured = false;

static IWin32InterceptionHandler* GetWin32InterceptionHandler()
{
	ID2DXContext* d2dxContext = D2DXContextFactory::GetInstance(false);

	if (!d2dxContext)
	{
		return nullptr;
	}

	return static_cast<IWin32InterceptionHandler*>(d2dxContext);
}

static ID2InterceptionHandler* GetD2InterceptionHandler()
{
	return D2DXContextFactory::GetInstance(false);
}

#ifdef D2DX_PROFILE
static DWORD
(WINAPI*
	SleepEx_Real)(
		_In_ DWORD dwMilliseconds,
		_In_ BOOL bAlertable
		) = SleepEx;

static DWORD WINAPI SleepEx_Hooked(
	_In_ DWORD dwMilliseconds,
	_In_ BOOL bAlertable)
{
	Timer _timer(ProfCategory::Sleep);
	return SleepEx_Real(dwMilliseconds, bAlertable);
}
#endif

COLORREF(WINAPI* GetPixel_real)(
	_In_ HDC hdc,
	_In_ int x,
	_In_ int y) = GetPixel;

COLORREF WINAPI GetPixel_Hooked(_In_ HDC hdc, _In_ int x, _In_ int y)
{
	/* Gets rid of the long delay on startup and when switching between menus in < 1.14,
	   as the game is doing a ton of pixel readbacks... for some reason. */
	return 0;
}

int (WINAPI* ShowCursor_Real)(
	_In_ BOOL bShow) = ShowCursor;

int WINAPI ShowCursor_Hooked(
	_In_ BOOL bShow)
{
	/* Override how the game hides/shows the cursor. We will take care of that. */
	return bShow ? 1 : -1;
}

BOOL(WINAPI* SetWindowPos_Real)(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags) = SetWindowPos;

BOOL
WINAPI
SetWindowPos_Hooked(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags)
{
	/* Stop the game from moving/sizing the window. */
	return SetWindowPos_Real(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags | SWP_NOSIZE | SWP_NOMOVE);
}

BOOL
(WINAPI*
	SetCursorPos_Real)(
		_In_ int X,
		_In_ int Y) = SetCursorPos;

BOOL
WINAPI
SetCursorPos_Hooked(
	_In_ int X,
	_In_ int Y)
{
	Timer _timer(ProfCategory::Detours);
	auto win32InterceptionHandler = GetWin32InterceptionHandler();

	if (!win32InterceptionHandler)
	{
		return FALSE;
	}

	auto adjustedPos = win32InterceptionHandler->OnSetCursorPos({ X, Y });

	if (adjustedPos.x < 0)
	{
		return FALSE;
	}

	return SetCursorPos_Real(adjustedPos.x, adjustedPos.y);
}

LRESULT
(WINAPI*
	SendMessageA_Real)(
		_In_ HWND hWnd,
		_In_ UINT Msg,
		_Pre_maybenull_ _Post_valid_ WPARAM wParam,
		_Pre_maybenull_ _Post_valid_ LPARAM lParam) = SendMessageA;

LRESULT
WINAPI
SendMessageA_Hooked(
	_In_ HWND hWnd,
	_In_ UINT Msg,
	_Pre_maybenull_ _Post_valid_ WPARAM wParam,
	_Pre_maybenull_ _Post_valid_ LPARAM lParam)
{
	Timer _timer(ProfCategory::Detours);
	if (Msg == WM_MOUSEMOVE)
	{
		auto win32InterceptionHandler = GetWin32InterceptionHandler();

		if (!win32InterceptionHandler)
		{
			return 0;
		}

		auto x = GET_X_LPARAM(lParam);
		auto y = GET_Y_LPARAM(lParam);

		auto adjustedPos = win32InterceptionHandler->OnMouseMoveMessage({ x, y });

		if (adjustedPos.x < 0)
		{
			return 0;
		}

		lParam = ((uint16_t)adjustedPos.y << 16) | ((uint16_t)adjustedPos.x & 0xFFFF);
	}

	return SendMessageA_Real(hWnd, Msg, wParam, lParam);
}

_Success_(return)
int
WINAPI
DrawTextA_Hooked(
	_In_ HDC hdc,
	_When_((format & DT_MODIFYSTRING), _At_((LPSTR)lpchText, _Inout_grows_updates_bypassable_or_z_(cchText, 4)))
	_When_((!(format & DT_MODIFYSTRING)), _In_bypassable_reads_or_z_(cchText))
	LPCSTR lpchText,
	_In_ int cchText,
	_Inout_ LPRECT lprc,
	_In_ UINT format)
{
	/* This removes the "weird characters" being printed by the game in the top left corner.
	   There is still a delay but the GetPixel hook takes care of that... */
	return 0;
}


_Success_(return)
int(
	WINAPI *
	DrawTextA_Real)(
		_In_ HDC hdc,
		_When_((format & DT_MODIFYSTRING), _At_((LPSTR)lpchText, _Inout_grows_updates_bypassable_or_z_(cchText, 4)))
		_When_((!(format & DT_MODIFYSTRING)), _In_bypassable_reads_or_z_(cchText))
		LPCSTR lpchText,
		_In_ int cchText,
		_Inout_ LPRECT lprc,
		_In_ UINT format) = DrawTextA;


typedef void(__cdecl* D2Client_DrawCursor)();
typedef void(__fastcall* D2Client_DrawShadow)(void*);
typedef void(__fastcall* D2Client_DrawUnit)(void*, int32_t, int32_t);
typedef void(__fastcall* D2Client_DrawInvItem)(void*, int32_t, int32_t);
typedef void(__fastcall* D2Client_DrawUnitOverlay)(void*, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void(__stdcall* D2Client_DrawUnitOverlay_111)(void*, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void(__stdcall* D2Gfx_DrawImage)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef void(__stdcall* D2Gfx_DrawFloor)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef BOOL(__stdcall* D2Gfx_DrawTile)(void*, int32_t, int32_t, int32_t, int32_t);
typedef BOOL(__stdcall* D2Gfx_DrawTileAlpha)(void*, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef BOOL(__stdcall* D2Gfx_DrawTileShadow)(void*, int32_t, int32_t, int32_t, int32_t);
typedef void(__cdecl* D2Win_DrawCursor)();
typedef void(__fastcall* D2Win_DrawChar)(void*, int32_t);
typedef void(__fastcall* D2Win_DrawText)(wchar_t*, int32_t, int32_t, int32_t, int32_t);

D2Client_DrawCursor D2Client_DrawCursor_Real = nullptr;
void* D2Client_DrawShadow_Real = nullptr;
void* D2Client_DrawUnit_Real = nullptr;
void* D2Client_DrawInvItem_Real = nullptr;
void* D2Client_DrawUnitOverlay_Real = nullptr;
D2Gfx_DrawImage D2Gfx_DrawImage_Real = nullptr;
D2Gfx_DrawFloor D2Gfx_DrawFloor_Real = nullptr;
D2Gfx_DrawTile D2Gfx_DrawTile_Real = nullptr;
D2Gfx_DrawTileAlpha D2Gfx_DrawTileAlpha_Real = nullptr;
D2Gfx_DrawTileShadow D2Gfx_DrawTileShadow_Real = nullptr;
void* D2Win_DrawCursor_Real = nullptr;
void* D2Win_DrawChar_Real = nullptr;
D2Win_DrawText D2Win_DrawText_Real = nullptr;

void __cdecl D2Client_DrawCursor_Hooked()
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetSurface(D2DX_SURFACE_CURSOR);
		D2Client_DrawCursor_Real();
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Client_DrawCursor_Real();
	}
}

void __fastcall D2Client_DrawShadow_Hooked(void* unit)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Client_DrawShadow)D2Client_DrawShadow_Real)(unit);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Client_DrawShadow)D2Client_DrawShadow_Real)(unit);
	}
}

void __fastcall D2Client_DrawShadow111_Impl(void* _1)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		__asm {
			mov eax, _1;
			call [D2Client_DrawShadow_Real]
		}
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		__asm {
			mov eax, _1;
			call [D2Client_DrawShadow_Real]
		}
	}
}

__declspec(naked) void D2Client_DrawShadow111_Hooked()
{
	__asm {
		mov ecx, eax
		call D2Client_DrawShadow111_Impl
		ret
	}
}

void __fastcall D2Client_DrawUnit_Hooked(void* unit, int32_t _1, int32_t _2)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Client_DrawUnit)D2Client_DrawUnit_Real)(unit, _1, _2);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Client_DrawUnit)D2Client_DrawUnit_Real)(unit, _1, _2);
	}
}

void __fastcall D2Client_DrawUnit111_Impl(int32_t _1, int32_t _2, int32_t _3)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		__asm {
			mov ecx, _1
			mov eax, _3
			push _2
			call [D2Client_DrawUnit_Real]
		}
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		__asm {
			mov ecx, _1
			mov eax, _3
			push _2
			call [D2Client_DrawUnit_Real]
		}
	}
}

__declspec(naked) void __stdcall D2Client_DrawUnit111_Hooked(int32_t _1)
{
	__asm {
		mov edx, [esp + 4]
		push eax
		call D2Client_DrawUnit111_Impl
		ret 4
	}
}

void __fastcall D2Client_DrawInvItem_Hooked(void* unit, int32_t _1, int32_t _2)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Client_DrawInvItem)D2Client_DrawInvItem_Real)(unit, _1, _2);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Client_DrawInvItem)D2Client_DrawInvItem_Real)(unit, _1, _2);
	}
}

void __fastcall D2Client_DrawInvItem111_Impl(int32_t _1, int32_t _2, int32_t _3)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		__asm {
			mov eax, _3;
			push _2;
			push _1;
			call [D2Client_DrawInvItem_Real]
		}
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		__asm {
			mov eax, _3;
			push _2;
			push _1;
			call [D2Client_DrawInvItem_Real]
		}
	}
}

__declspec(naked) void __stdcall D2Client_DrawInvItem111_Hooked(int32_t _1, int32_t _2)
{
	__asm {
		mov ecx, [esp+4]
		mov edx, [esp+8]
		push eax
		call D2Client_DrawInvItem111_Impl
		ret 8
	}
}

void __fastcall D2Client_DrawUnitOverlay_Hooked(void* unit, int32_t _1, int32_t _2, int32_t _3, int32_t _4, int32_t _5)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Client_DrawUnitOverlay)D2Client_DrawUnitOverlay_Real)(unit, _1, _2, _3, _4, _5);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Client_DrawUnitOverlay)D2Client_DrawUnitOverlay_Real)(unit, _1, _2, _3, _4, _5);
	}
}

void __stdcall D2Client_DrawUnitOverlay111_Hooked(void* unit, int32_t _1, int32_t _2, int32_t _3, int32_t _4, int32_t _5)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Client_DrawUnitOverlay_111)D2Client_DrawUnitOverlay_Real)(unit, _1, _2, _3, _4, _5);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Client_DrawUnitOverlay_111)D2Client_DrawUnitOverlay_Real)(unit, _1, _2, _3, _4, _5);
	}
}

void __stdcall D2Gfx_DrawFloor_Hooked(void* tile, void* _1, int32_t _2, int32_t _3, int32_t _4, int32_t _5, int32_t _6, int32_t _7, int32_t _8)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		D2Gfx_DrawFloor_Real(tile, _1, _2, _3, _4, _5, _6, _7, _8);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Gfx_DrawFloor_Real(tile, _1, _2, _3, _4, _5, _6, _7, _8);
	}
}

void __stdcall D2Gfx_DrawTile_Hooked(void* tile, int32_t _1, int32_t _2, int32_t _3, int32_t _4)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		D2Gfx_DrawTile_Real(tile, _1, _2, _3, _4);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Gfx_DrawTile_Real(tile, _1, _2, _3, _4);
	}
}

void __stdcall D2Gfx_DrawTileAlpha_Hooked(void* tile, int32_t _1, int32_t _2, int32_t _3, int32_t _4, int32_t _5)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		D2Gfx_DrawTileAlpha_Real(tile, _1, _2, _3, _4, _5);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Gfx_DrawTileAlpha_Real(tile, _1, _2, _3, _4, _5);
	}
}

void __stdcall D2Gfx_DrawTileShadow_Hooked(void* tile, int32_t _1, int32_t _2, int32_t _3, int32_t _4)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		D2Gfx_DrawTileShadow_Real(tile, _1, _2, _3, _4);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Gfx_DrawTileShadow_Real(tile, _1, _2, _3, _4);
	}
}

void __stdcall D2Win_DrawCursor_Impl(int32_t _1, int32_t _2, int32_t _3, int32_t _4, int32_t _5, int32_t _6)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetSurface(D2DX_SURFACE_CURSOR);
		D2Gfx_DrawImage_Real(_1, _2, _3, _4, _5, _6);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		D2Gfx_DrawImage_Real(_1, _2, _3, _4, _5, _6);
	}
}

static void* D2Win_DrawCursor_Ret = nullptr;
__declspec(naked) void D2Win_DrawCursor_Hooked()
{
	__asm {
		call D2Win_DrawCursor_Impl
		mov eax, D2Win_DrawCursor_Ret
		jmp eax
	}
}

void __cdecl D2Win_DrawCursor111_Hooked()
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetSurface(D2DX_SURFACE_CURSOR);
		((D2Win_DrawCursor)D2Win_DrawCursor_Real)();
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Win_DrawCursor)D2Win_DrawCursor_Real)();
	}
}

void __fastcall D2Win_DrawChar_Hooked(void* _1, int32_t _2)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		((D2Win_DrawChar)D2Win_DrawChar_Real)(_1, _2);
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		((D2Win_DrawChar)D2Win_DrawChar_Real)(_1, _2);
	}
}

void __fastcall D2Win_DrawChar111_Impl(int32_t _1, int32_t _2)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		uint16_t prev = d2InterceptionHandler->SetNewSurface();
		__asm {
			mov ebx, _1;
			push _2;
			call [D2Win_DrawChar_Real]
		}
		d2InterceptionHandler->SetSurface(prev);
	}
	else
	{
		__asm {
			mov ebx, _1;
			push _2;
			call [D2Win_DrawChar_Real]
		}
	}
}

__declspec(naked) void __stdcall D2Win_DrawChar111_Hooked(int32_t _1)
{
	__asm {
		mov ecx, ebx
		mov edx, [esp+4]
		call D2Win_DrawChar111_Impl
		ret 4
	}
}

void __fastcall D2Win_DrawText_Hooked(wchar_t* s, int32_t _1, int32_t _2, int32_t _3, int32_t _4)
{
	auto d2InterceptionHandler = GetD2InterceptionHandler();
	if (d2InterceptionHandler)
	{
		d2InterceptionHandler->InterceptDrawText(s);
	}
	D2Win_DrawText_Real(s, _1, _2, _3, _4);
}

static void* DrawShadowHook = nullptr;
static void* DrawUnitHook = nullptr;
static void* DrawInvItemHook = nullptr;
static void* DrawUnitOverlayHook = nullptr;
static void* DrawMenuCursorHook = nullptr;
static void* DrawMenuCharHook = nullptr;

_Use_decl_annotations_
void d2dx::AttachDetours(
	GameHelper& gameHelper,
	ID2DXContext& d2dxContext)
{
	if (hasDetoured)
	{
		return;
	}
	hasDetoured = true;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)DrawTextA_Real, DrawTextA_Hooked);
	DetourAttach(&(PVOID&)GetPixel_real, GetPixel_Hooked);
	DetourAttach(&(PVOID&)SendMessageA_Real, SendMessageA_Hooked);
	DetourAttach(&(PVOID&)ShowCursor_Real, ShowCursor_Hooked);
	DetourAttach(&(PVOID&)SetCursorPos_Real, SetCursorPos_Hooked);
	DetourAttach(&(PVOID&)SetWindowPos_Real, SetWindowPos_Hooked);
#ifdef D2DX_PROFILE
	DetourAttach(&(PVOID&)SleepEx_Real, SleepEx_Hooked);
#endif
	LONG lError = DetourTransactionCommit();
	if (lError != NO_ERROR) {
		D2DX_FATAL_ERROR("Failed to detour Win32 functions.");
	}

	if (!d2dxContext.GetOptions().GetFlag(OptionsFlag::NoAntiAliasing))
	{
		switch (gameHelper.gameVersion)
		{
		case GameVersion::Lod109d:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.d2ClientDll + 0xb58f0);
			D2Client_DrawShadow_Real = (char*)gameHelper.d2ClientDll + 0xb71e0;
			D2Client_DrawUnit_Real = (char*)gameHelper.d2ClientDll + 0xab9b0;
			D2Client_DrawInvItem_Real = (char*)gameHelper.d2ClientDll + 0xb7f60;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.d2ClientDll + 0xb80e0;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10072));
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10079));
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10080));
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10081));
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10082));
			D2Win_DrawCursor_Real = (char*)gameHelper.d2WinDll + 0xf4cb;
			D2Win_DrawCursor_Ret = (char*)gameHelper.d2WinDll + 0xf4d0;
			D2Win_DrawChar_Real = (char*)gameHelper.d2WinDll + 0x1830;
			D2Win_DrawText_Real = (D2Win_DrawText)GetProcAddress((HMODULE)gameHelper.d2WinDll, MAKEINTRESOURCEA(10117));

			DrawShadowHook = &D2Client_DrawShadow_Hooked;
			DrawUnitHook = &D2Client_DrawUnit_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar_Hooked;
			break;

		case GameVersion::Lod110f:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.d2ClientDll + 0xb7ac0);
			D2Client_DrawShadow_Real = (char*)gameHelper.d2ClientDll + 0xb9670;
			D2Client_DrawUnit_Real = (char*)gameHelper.d2ClientDll + 0xadb40;
			D2Client_DrawInvItem_Real = (char*)gameHelper.d2ClientDll + 0xba320;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.d2ClientDll + 0xba490;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10072));
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10079));
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10080));
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10081));
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10082));
			D2Win_DrawCursor_Real = (char*)gameHelper.d2WinDll + 0xd92b;
			D2Win_DrawCursor_Ret = (char*)gameHelper.d2WinDll + 0xd930;
			D2Win_DrawChar_Real = (char*)gameHelper.d2WinDll + 0x1890;
			D2Win_DrawText_Real = (D2Win_DrawText)GetProcAddress((HMODULE)gameHelper.d2WinDll, MAKEINTRESOURCEA(10117));

			DrawShadowHook = &D2Client_DrawShadow_Hooked;
			DrawUnitHook = &D2Client_DrawUnit_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar_Hooked;
			break;

		case GameVersion::Lod112:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.d2ClientDll + 0x9f5b0);
			D2Client_DrawShadow_Real = (char*)gameHelper.d2ClientDll + 0x93860;
			D2Client_DrawUnit_Real = (char*)gameHelper.d2ClientDll + 0xbba30;
			D2Client_DrawInvItem_Real = (char*)gameHelper.d2ClientDll + 0x93380;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.d2ClientDll + 0x92cc0;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10024));
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10038));
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10014));
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10075));
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10042));
			D2Win_DrawCursor_Real = (char*)gameHelper.d2WinDll + 0xcd80;
			D2Win_DrawChar_Real = (char*)gameHelper.d2WinDll + 0x16140;
			D2Win_DrawText_Real = (D2Win_DrawText)GetProcAddress((HMODULE)gameHelper.d2WinDll, MAKEINTRESOURCEA(10001));

			DrawShadowHook = &D2Client_DrawShadow111_Hooked;
			DrawUnitHook = &D2Client_DrawUnit111_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem111_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay111_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor111_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar111_Hooked;
			break;

		case GameVersion::Lod113c:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.d2ClientDll + 0x16a90);
			D2Client_DrawShadow_Real = (char*)gameHelper.d2ClientDll + 0x6ba90;
			D2Client_DrawUnit_Real = (char*)gameHelper.d2ClientDll + 0x666d0;
			D2Client_DrawInvItem_Real = (char*)gameHelper.d2ClientDll + 0x6b5a0;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.d2ClientDll + 0x6aee0;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10041));
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10076));
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10001));
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10058));
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10069));
			D2Win_DrawCursor_Real = (char*)gameHelper.d2WinDll + 0x17f40;
			D2Win_DrawChar_Real = (char*)gameHelper.d2WinDll + 0xe5f0;
			D2Win_DrawText_Real = (D2Win_DrawText)GetProcAddress((HMODULE)gameHelper.d2WinDll, MAKEINTRESOURCEA(10150));

			DrawShadowHook = &D2Client_DrawShadow111_Hooked;
			DrawUnitHook = &D2Client_DrawUnit111_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem111_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay111_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor111_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar111_Hooked;
			break;

		case GameVersion::Lod113d:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.d2ClientDll + 0x14f10);
			D2Client_DrawShadow_Real = (char*)gameHelper.d2ClientDll + 0x5f970;
			D2Client_DrawUnit_Real = (char*)gameHelper.d2ClientDll + 0xb4b50;
			D2Client_DrawInvItem_Real = (char*)gameHelper.d2ClientDll + 0x5f480;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.d2ClientDll + 0x5edc0;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10042));
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10001));
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10081));
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10040));
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)GetProcAddress((HMODULE)gameHelper.d2GfxDll, MAKEINTRESOURCEA(10060));
			D2Win_DrawCursor_Real = (char*)gameHelper.d2WinDll + 0xe0c0;
			D2Win_DrawChar_Real = (char*)gameHelper.d2WinDll + 0xbfe0;
			D2Win_DrawText_Real = (D2Win_DrawText)GetProcAddress((HMODULE)gameHelper.d2WinDll, MAKEINTRESOURCEA(10076));

			DrawShadowHook = &D2Client_DrawShadow111_Hooked;
			DrawUnitHook = &D2Client_DrawUnit_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem111_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay111_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor111_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar111_Hooked;
			break;

		case GameVersion::Lod114d:
			D2Client_DrawCursor_Real = (D2Client_DrawCursor)((char*)gameHelper.gameExe + 0x684c0);
			D2Client_DrawShadow_Real = (char*)gameHelper.gameExe + 0x71620;
			D2Client_DrawUnit_Real = (char*)gameHelper.gameExe + 0xdc7b0;
			D2Client_DrawInvItem_Real = (char*)gameHelper.gameExe + 0x6ee80;
			D2Client_DrawUnitOverlay_Real = (char*)gameHelper.gameExe + 0x6e300;
			D2Gfx_DrawImage_Real = (D2Gfx_DrawImage)((char*)gameHelper.gameExe + 0xf6480);
			D2Gfx_DrawFloor_Real = (D2Gfx_DrawFloor)((char*)gameHelper.gameExe + 0xf68e0);
			D2Gfx_DrawTile_Real = (D2Gfx_DrawTile)((char*)gameHelper.gameExe + 0xf6920);
			D2Gfx_DrawTileAlpha_Real = (D2Gfx_DrawTileAlpha)((char*)gameHelper.gameExe + 0xf6950);
			D2Gfx_DrawTileShadow_Real = (D2Gfx_DrawTileShadow)((char*)gameHelper.gameExe + 0xf6980);
			D2Win_DrawCursor_Real = (char*)gameHelper.gameExe + 0xf97e0;
			D2Win_DrawChar_Real = (char*)gameHelper.gameExe + 0x103ba0;
			D2Win_DrawText_Real = (D2Win_DrawText)((char*)gameHelper.gameExe + 0x102320);
			
			DrawShadowHook = &D2Client_DrawShadow_Hooked;
			DrawUnitHook = &D2Client_DrawUnit_Hooked;
			DrawInvItemHook = &D2Client_DrawInvItem_Hooked;
			DrawUnitOverlayHook = &D2Client_DrawUnitOverlay111_Hooked;
			DrawMenuCursorHook = &D2Win_DrawCursor111_Hooked;
			DrawMenuCharHook = &D2Win_DrawChar_Hooked;
			break;

		default:
			D2DX_LOG("Unsupported game version for anti-aliasing.");
			d2dxContext.GetOptions().SetFlag(OptionsFlag::NoAntiAliasing, true);
			return;
		}

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((void**)&D2Client_DrawCursor_Real, D2Client_DrawCursor_Hooked);
		DetourAttach(&D2Client_DrawShadow_Real, DrawShadowHook);
		DetourAttach(&D2Client_DrawUnit_Real, DrawUnitHook);
		DetourAttach(&D2Client_DrawInvItem_Real, DrawInvItemHook);
		DetourAttach(&D2Client_DrawUnitOverlay_Real, DrawUnitOverlayHook);
		DetourAttach((void**)&D2Gfx_DrawFloor_Real, D2Gfx_DrawFloor_Hooked);
		DetourAttach((void**)&D2Gfx_DrawTile_Real, D2Gfx_DrawTile_Hooked);
		DetourAttach((void**)&D2Gfx_DrawTileAlpha_Real, D2Gfx_DrawTileAlpha_Hooked);
		DetourAttach((void**)&D2Gfx_DrawTileShadow_Real, D2Gfx_DrawTileShadow_Hooked);
		DetourAttach((void**)&D2Win_DrawCursor_Real, DrawMenuCursorHook);
		DetourAttach(&D2Win_DrawChar_Real, DrawMenuCharHook);
		DetourAttach((void**)&D2Win_DrawText_Real, D2Win_DrawText_Hooked);
		LONG lError = DetourTransactionCommit();
		if (lError != NO_ERROR) {
			D2DX_LOG("Failed to detour anti-aliasing functions: %i.", lError);
			d2dxContext.GetOptions().SetFlag(OptionsFlag::NoAntiAliasing, true);
		}
	}
}

_Use_decl_annotations_
void d2dx::DetachDetours(
	GameHelper& gameHelper,
	ID2DXContext& d2dxContext)
{
	if (!hasDetoured)
	{
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)DrawTextA_Real, DrawTextA_Hooked);
	DetourDetach(&(PVOID&)GetPixel_real, GetPixel_Hooked);
	DetourDetach(&(PVOID&)SendMessageA_Real, SendMessageA_Hooked);
	DetourDetach(&(PVOID&)ShowCursor_Real, ShowCursor_Hooked);
	DetourDetach(&(PVOID&)SetCursorPos_Real, SetCursorPos_Hooked);
	DetourDetach(&(PVOID&)SetWindowPos_Real, SetWindowPos_Hooked);
#ifdef D2DX_PROFILE
	DetourDetach(&(PVOID&)SleepEx_Real, SleepEx_Hooked);
#endif

	LONG lError = DetourTransactionCommit();

	if (lError != NO_ERROR)
	{
		/* An error here doesn't really matter. The process is going. */
		return;
	}
	hasDetoured = false;

	if (!d2dxContext.GetOptions().GetFlag(OptionsFlag::NoAntiAliasing))
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach((void**)&D2Client_DrawCursor_Real, D2Client_DrawCursor_Hooked);
		DetourDetach(&D2Client_DrawShadow_Real, DrawShadowHook);
		DetourDetach(&D2Client_DrawUnit_Real, DrawUnitHook);
		DetourDetach(&D2Client_DrawInvItem_Real, DrawInvItemHook);
		DetourDetach(&D2Client_DrawUnitOverlay_Real, DrawUnitOverlayHook);
		DetourDetach((void**)&D2Gfx_DrawFloor_Real, D2Gfx_DrawFloor_Hooked);
		DetourDetach((void**)&D2Gfx_DrawTile_Real, D2Gfx_DrawTile_Hooked);
		DetourDetach((void**)&D2Gfx_DrawTileAlpha_Real, D2Gfx_DrawTileAlpha_Hooked);
		DetourDetach((void**)&D2Gfx_DrawTileShadow_Real, D2Gfx_DrawTileShadow_Hooked);
		DetourDetach((void**)&D2Win_DrawCursor_Real, DrawMenuCursorHook);
		DetourDetach(&D2Win_DrawChar_Real, DrawMenuCharHook);
		DetourDetach((void**)&D2Win_DrawText_Real, D2Win_DrawText_Hooked);
		DetourTransactionCommit();
	}
}
