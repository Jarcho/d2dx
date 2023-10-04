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
	L"WIN8"
	L"WIN7",
	L"VISTASP2",
	L"VISTASP1",
	L"VISTA",
	L"WINXPSP3",
	L"WINXPSP2",
	L"WINXPSP1",
	L"WINXP",
	L"WIN98",
	L"WIN95",
};

const wchar_t* CompatModeKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

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
	switch (result)
	{
	case ERROR_SUCCESS:
		break;

	case ERROR_FILE_NOT_FOUND:
		D2DX_LOG("nf");
		return CompatModeState::Disabled;

	default:
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
	
	CompatModeState machine_state = CheckCompatModeReg(HKEY_LOCAL_MACHINE, path.data(), false);
	if (machine_state == CompatModeState::Enabled)
	{
		return machine_state;
	}

	CompatModeState user_state = CheckCompatModeReg(HKEY_CURRENT_USER, path.data(), remove);
	if (user_state == CompatModeState::Enabled || user_state == CompatModeState::Updated)
	{
		return user_state;
	}

	if (machine_state == CompatModeState::Disabled)
	{
		return machine_state;
	}

	auto reportedWindowsVersion = d2dx::GetWindowsVersion();
	auto realWindowsVersion = d2dx::GetActualWindowsVersion();

	return reportedWindowsVersion.major > 6 || (reportedWindowsVersion.major == 6 && reportedWindowsVersion.minor >= 3)
		? CompatModeState::Disabled // Reported >= Win10
		: realWindowsVersion.major == 0
		? CompatModeState::Unknown // No real version
		: reportedWindowsVersion.major < realWindowsVersion.major
		|| (reportedWindowsVersion.major == realWindowsVersion.major && reportedWindowsVersion.minor < realWindowsVersion.minor)
		? CompatModeState::Enabled // reported < real
		: CompatModeState::Disabled; // reported >= real
}
