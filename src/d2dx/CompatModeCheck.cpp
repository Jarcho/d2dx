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
#include "CompatModeCheck.h"
#include "Utils.h"

using namespace d2dx;

static const wchar_t* CompatModeStrings[] =
{
	L"WIN8RTM",
	L"WIN7RTM",
	L"VISTASP2",
	L"VISTASP1",
	L"VISTARTM",
	L"WINXPSP3",
	L"WINXPSP2",
	L"WIN98",
	L"WIN95",
};

CompatModeState CheckCompatModeReg(
	_In_ HKEY hRootKey,
	_In_ const wchar_t* path,
	_In_ bool remove)
{
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(
		hRootKey,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
		0,
		KEY_QUERY_VALUE | KEY_WOW64_64KEY | (remove ? KEY_SET_VALUE : 0),
		&hKey);
	if (result != ERROR_SUCCESS)
	{
		return CompatModeState::Unknown;
	}

	std::wstring buf;
	DWORD len = MAX_PATH * 2;
	do
	{
		buf.resize(len / 2);
		result = RegGetValueW(
			hKey,
			nullptr,
			path,
			RRF_RT_REG_SZ,
			nullptr,
			buf.data(),
			&len);
	} while (result == ERROR_MORE_DATA);
	if (result != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return CompatModeState::Unknown;
	}
	buf.resize(len / 2);

	CompatModeState ret = CompatModeState::Disabled;
	switch (result)
	{
	case ERROR_SUCCESS:
		for (const wchar_t* s : CompatModeStrings)
		{
			std::size_t pos = buf.find(s);
			if (pos != std::wstring::npos)
			{
				ret = CompatModeState::Enabled;
				if (remove)
				{
					std::size_t len = wcslen(s);
					if (pos != 0 && buf[pos - 1] == L' ')
					{
						--pos;
						++len;
					}
					else if (pos + len < buf.length() && buf[pos + len] == L' ')
					{
						++len;
					}
					else
					{
						continue;
					}

					if (len + 2 == buf.length() && pos == 1 && buf[0] == L'~')
					{
						result = RegDeleteValueW(hKey, path);
					}
					else
					{
						buf.erase(pos, len);
						result = RegSetValueExW(
							hKey,
							path,
							0,
							REG_SZ,
							reinterpret_cast<const BYTE*>(buf.data()),
							buf.length() * 2);
					}
					if (result == ERROR_SUCCESS)
					{
						ret = CompatModeState::Updated;
					}
				}
				break;
			}
		}
		break;

	case ERROR_FILE_NOT_FOUND:
		break;

	default:
		ret = CompatModeState::Unknown;
		break;
	}

	RegCloseKey(hKey);
	return ret;
}

_Use_decl_annotations_
CompatModeState d2dx::CheckCompatMode(bool remove)
{
	auto version = d2dx::GetWindowsVersion();
	auto realVersion = d2dx::GetActualWindowsVersion();

	if (realVersion.major == version.major && realVersion.minor == version.minor)
	{
		return CompatModeState::Disabled;
	}
	if (version.major > 6 || (version.major == 6 && version.minor >= 3))
	{
		// Compat mode is set to at least win10.
		return CompatModeState::Disabled;
	}
	
	HMODULE module = GetModuleHandleW(nullptr);
	std::vector<wchar_t> path;
	DWORD result = MAX_PATH / 2;
	do
	{
		result *= 2;
		path.resize(result);
		result = GetModuleFileNameW(module, path.data(), path.size());
		if (result == 0)
		{
			return CompatModeState::Unknown;
		}
	} while (result == path.size());
	path.resize(static_cast<std::size_t>(result + 1));
	
	CompatModeState ustate = CheckCompatModeReg(HKEY_CURRENT_USER, path.data(), remove);
	if (ustate == CompatModeState::Enabled || ustate == CompatModeState::Updated)
	{
		return ustate;
	}

	CompatModeState mstate = CheckCompatModeReg(HKEY_LOCAL_MACHINE, path.data(), false);
	if (mstate == CompatModeState::Enabled)
	{
		return mstate;
	}

	return realVersion.major == 0 && mstate == CompatModeState::Unknown && ustate == CompatModeState::Unknown
		? CompatModeState::Unknown
		: CompatModeState::Enabled;
}
