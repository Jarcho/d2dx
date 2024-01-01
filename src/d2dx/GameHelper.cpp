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
#include "GameHelper.h"
#include "Buffer.h"
#include "Utils.h"

using namespace d2dx;


GameVersion GetGameVersion()
{
	GameVersion version = GameVersion::Unsupported;

	auto versionSize = GetFileVersionInfoSizeA("game.exe", nullptr);
	Buffer<uint8_t> verData(versionSize);

	if (!GetFileVersionInfoA("game.exe", NULL, verData.capacity, verData.items))
	{
		D2DX_LOG("Failed to get file version for game.exe.");
		return GameVersion::Unsupported;
	}

	uint32_t size = 0;
	const uint8_t* lpBuffer = nullptr;
	bool success = VerQueryValueA(verData.items, "\\", (VOID FAR * FAR*) & lpBuffer, &size);

	if (!(success && size > 0))
	{
		D2DX_LOG("Failed to query version info for game.exe.");
		return GameVersion::Unsupported;
	}

	VS_FIXEDFILEINFO* vsFixedFileInfo = (VS_FIXEDFILEINFO*)lpBuffer;
	if (vsFixedFileInfo->dwSignature != 0xfeef04bd)
	{
		D2DX_LOG("Unexpected signature in version info for game.exe.");
		return GameVersion::Unsupported;
	}

	const int32_t a = vsFixedFileInfo->dwFileVersionMS >> 16;
	const int32_t b = vsFixedFileInfo->dwFileVersionMS & 0xffff;
	const int32_t c = vsFixedFileInfo->dwFileVersionLS >> 16;
	const int32_t d = vsFixedFileInfo->dwFileVersionLS & 0xffff;

	if (a == 1 && b == 0 && c == 9 && d == 22)
	{
		version = GameVersion::Lod109d;
	}
	else if (a == 1 && b == 0 && c == 10 && d == 39)
	{
		version = GameVersion::Lod110f;
	}
	else if (a == 1 && b == 0 && c == 12 && d == 49)
	{
		version = GameVersion::Lod112;
	}
	else if (a == 1 && b == 0 && c == 13 && d == 60)
	{
		version = GameVersion::Lod113c;
	}
	else if (a == 1 && b == 0 && c == 13 && d == 64)
	{
		version = GameVersion::Lod113d;
	}
	else if (a == 1 && b == 14 && c == 3 && d == 68)
	{
		D2DX_FATAL_ERROR("This version (1.14b) of Diablo II will not work with D2DX. Please upgrade to version 1.14d.");
	}
	else if (a == 1 && b == 14 && c == 3 && d == 71)
	{
		version = GameVersion::Lod114d;
	}

	D2DX_LOG("Game version: %d.%d.%d.%d (%s)\n", a, b, c, d, version == GameVersion::Unsupported ? "unsupported" : "supported");

	return version;
}


GameHelper::GameHelper() :
	gameVersion(GetGameVersion()),
	process(GetCurrentProcess()),
	gameExe(GetModuleHandleA("game.exe")),
	d2ClientDll(LoadLibraryA("D2Client.dll")),
	d2CommonDll(LoadLibraryA("D2Common.dll")),
	d2GfxDll(LoadLibraryA("D2Gfx.dll")),
	d2WinDll(LoadLibraryA("D2Win.dll")),
	isProjectDiablo2(GetModuleHandleA("PD2_EXT.dll") != nullptr)
{
	if (isProjectDiablo2)
	{
		D2DX_LOG("Detected Project Diablo 2.");
	}
}

_Use_decl_annotations_
const char* GameHelper::GetVersionString() const
{
	switch (gameVersion)
	{
	case GameVersion::Lod109d:
		return "Lod109d";
	case GameVersion::Lod110f:
		return "Lod110";
	case GameVersion::Lod112:
		return "Lod112";
	case GameVersion::Lod113c:
		return "Lod113c";
	case GameVersion::Lod113d:
		return "Lod113d";
	case GameVersion::Lod114d:
		return "Lod114d";
	default:
		return "Unhandled";
	}
}

Size GameHelper::GetConfiguredGameSize() const
{
	HKEY hKey;
	LPCTSTR diablo2Key = TEXT("SOFTWARE\\Blizzard Entertainment\\Diablo II");
	LONG openRes = RegOpenKeyEx(HKEY_CURRENT_USER, diablo2Key, 0, KEY_READ, &hKey);
	if (openRes != ERROR_SUCCESS)
	{
		return { 800, 600 };
	}

	DWORD type = REG_DWORD;
	DWORD size = 4;
	DWORD value = 0;
	auto queryRes = RegQueryValueExA(hKey, "Resolution", NULL, &type, (LPBYTE)&value, &size);
	assert(queryRes == ERROR_SUCCESS || queryRes == ERROR_MORE_DATA);

	RegCloseKey(hKey);

	if (value == 0)
	{
		return { 640, 480 };
	}
	else
	{
		return { 800, 600 };
	}
}
