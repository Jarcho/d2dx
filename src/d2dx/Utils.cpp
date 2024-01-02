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
#include "Utils.h"
#include "Profiler.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#include "../../thirdparty/stb_image/stb_image_write.h"

#define POCKETLZMA_LZMA_C_DEFINE
#include "../../thirdparty/pocketlzma/pocketlzma.hpp"

#pragma comment(lib, "Netapi32.lib")

using namespace d2dx;

namespace
{
    static int64_t query_frequency() noexcept
    {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        return li.QuadPart;
    }

    static double inv_frequency() noexcept
    {
        static double _freq = 1. / (static_cast<double>(query_frequency()) / 1000.);
        return _freq;
    }
}

int64_t d2dx::TimeStamp() noexcept
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (int64_t)li.QuadPart;
}

double d2dx::TimeToMs(int64_t time) noexcept
{
    return static_cast<double>(time) * inv_frequency();
}

#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

static WindowsVersion* windowsVersion = nullptr;

WindowsVersion d2dx::GetWindowsVersion()
{
    if (windowsVersion)
    {
        return *windowsVersion;
    }

    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod)
    {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr)
        {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (STATUS_SUCCESS == fxPtr(&rovi))
            {
                windowsVersion = new WindowsVersion();
                windowsVersion->major = rovi.dwMajorVersion;
                windowsVersion->minor = rovi.dwMinorVersion;
                windowsVersion->build = rovi.dwBuildNumber;
                return *windowsVersion;
            }
        }
    }
    WindowsVersion wv = { 0,0,0 };
    return wv;
}

WindowsVersion d2dx::GetActualWindowsVersion()
{
    LPSERVER_INFO_101 bufptr = nullptr;

    DWORD result = NetServerGetInfo(nullptr, 101, (LPBYTE*)&bufptr);

    if (!bufptr)
    {
        return { 0,0,0 };
    }

    WindowsVersion windowsVersion{ bufptr->sv101_version_major, bufptr->sv101_version_minor, 0 };
    NetApiBufferFree(bufptr);
    return windowsVersion;
}

static bool logFileOpened = false;
static FILE* logFile = nullptr;
static CRITICAL_SECTION logFileCS;

static void EnsureLogFileOpened()
{
    if (!logFileOpened)
    {
        logFileOpened = true;
        if (fopen_s(&logFile, "d2dx_log.txt", "w") != 0)
        {
            logFile = nullptr;
        }
        InitializeCriticalSection(&logFileCS);
    }
}

static DWORD WINAPI WriteToLogFileWorkItemFunc(PVOID pvContext)
{
    HaltSleepProfile _halt;
    char* s = (char*)pvContext;

    OutputDebugStringA(s);

    EnterCriticalSection(&logFileCS);

    if (logFile)
    {
        fwrite(s, strlen(s), 1, logFile);
        fflush(logFile);
    }

    LeaveCriticalSection(&logFileCS);

    free(s);

    return 0;
}

_Use_decl_annotations_
void d2dx::detail::Log(
    const char* s)
{
    HaltSleepProfile _halt;
    EnsureLogFileOpened();
    QueueUserWorkItem(WriteToLogFileWorkItemFunc, _strdup(s), WT_EXECUTEDEFAULT);
}

_Use_decl_annotations_
Buffer<char> d2dx::ReadTextFile(
    const char* filename)
{
    FILE* cfgFile = nullptr;

    errno_t err = fopen_s(&cfgFile, "d2dx.cfg", "r");

    if (err < 0 || !cfgFile)
    {
        Buffer<char> str(1);
        str.items[0] = 0;
        return str;
    }

    fseek(cfgFile, 0, SEEK_END);

    long size = ftell(cfgFile);

    if (size <= 0)
    {
        Buffer<char> str(1);
        str.items[0] = 0;
        fclose(cfgFile);
        return str;
    }

    fseek(cfgFile, 0, SEEK_SET);

    Buffer<char> str(size + 1, true);

    fread_s(str.items, str.capacity, size, 1, cfgFile);
    
    str.items[size] = 0;

    fclose(cfgFile);

    return str;
}

_Use_decl_annotations_
char* d2dx::detail::GetMessageForHRESULT(
    HRESULT hr,
    const char* func,
    int32_t line) noexcept
{
    static Buffer<char> buffer(4096);
    auto msg = std::system_category().message(hr);
    sprintf_s(buffer.items, buffer.capacity, "%s line %i\nHRESULT: 0x%.8lx\nMessage: %s", func, line, (unsigned long)hr, msg.c_str());
    return buffer.items;
}

_Use_decl_annotations_
void d2dx::detail::ThrowFromHRESULT(
    HRESULT hr,
    const char* func,
    int32_t line)
{
#ifndef NDEBUG
    __debugbreak();
#endif

    throw ComException(hr, func, line);
}

void d2dx::detail::FatalException() noexcept
{
    try
    {
        std::rethrow_exception(std::current_exception());
    }
    catch (const std::exception& e)
    {
        D2DX_LOG("%s", e.what());
        MessageBoxA(nullptr, e.what(), "D2DX Fatal Error", MB_OK | MB_ICONSTOP);
        TerminateProcess(GetCurrentProcess(), -1);
    }
}

_Use_decl_annotations_
void d2dx::detail::FatalError(
    const char* msg) noexcept
{
    D2DX_LOG("%s", msg);
    MessageBoxA(nullptr, msg, "D2DX Fatal Error", MB_OK | MB_ICONSTOP);
    TerminateProcess(GetCurrentProcess(), -1);
}

_Use_decl_annotations_
void d2dx::DumpTexture(
    uint64_t hash,
    int32_t w,
    int32_t h,
    const uint8_t* pixels,
    uint32_t pixelsSize,
    const uint32_t* palette)
{
    char s[256];

    if (!std::filesystem::exists("dump"))
    {
        std::filesystem::create_directory("dump");
    }

    sprintf_s(s, "dump/%016llx.bmp", hash);

    if (std::filesystem::exists(s))
    {
        return;
    }

    Buffer<uint32_t> data(w * h * 4);

    for (int32_t j = 0; j < h; ++j)
    {
        for (int32_t i = 0; i < w; ++i)
        {
            uint8_t idx = pixels[i + j * w];
            uint32_t c = palette[idx] | 0xFF000000;
        
            c = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);

            data.items[i + j * w] = c;
        }
    }
    stbi_write_bmp(s, w, h, 4, data.items);
}

_Use_decl_annotations_
bool d2dx::DecompressLZMAToFile(
    const uint8_t* data,
    uint32_t dataSize,
    const char* filename)
{
    bool succeeded = true;
    plz::PocketLzma p;
    std::vector<uint8_t> decompressed;
    HANDLE file = nullptr;
    DWORD bytesWritten = 0;

    auto status = p.decompress(data, dataSize, decompressed);

    if (status != plz::StatusCode::Ok)
    {
        succeeded = false;
        goto end;
    }

    file = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!file)
    {
        succeeded = false;
        goto end;
    }

    if (!WriteFile(file, decompressed.data(), decompressed.size(), &bytesWritten, nullptr))
    {
        succeeded = false;
        goto end;
    }

end:
    if (file)
    {
        CloseHandle(file);
    }

    return succeeded;
}

_Use_decl_annotations_
int32_t d2dx::IndexOfUInt64(
    const uint64_t* __restrict items,
    uint32_t itemsCount,
    uint64_t item)
{
    assert(items && ((uintptr_t)items & 0x1F) == 0);
    assert(!(itemsCount & 0x1F));

    const __m128i key4 = _mm_set1_epi64x(item);

    int32_t findIndex = -1;
    uint32_t i = 0;
    uint64_t res = 0;

    /* Note: don't tweak this loop. It manages to fit within the XMM registers on x86
       and any change could cause temporaries to spill onto the stack. */

    for (; i < itemsCount; i += 32)
    {
        const __m128i ck0 = _mm_load_si128((const __m128i*) & items[i + 0]);
        const __m128i ck1 = _mm_load_si128((const __m128i*) & items[i + 2]);
        const __m128i ck2 = _mm_load_si128((const __m128i*) & items[i + 4]);
        const __m128i ck3 = _mm_load_si128((const __m128i*) & items[i + 6]);
        const __m128i ck4 = _mm_load_si128((const __m128i*) & items[i + 8]);
        const __m128i ck5 = _mm_load_si128((const __m128i*) & items[i + 10]);
        const __m128i ck6 = _mm_load_si128((const __m128i*) & items[i + 12]);
        const __m128i ck7 = _mm_load_si128((const __m128i*) & items[i + 14]);

        const __m128i cmp0 = _mm_cmpeq_epi64(key4, ck0);
        const __m128i cmp1 = _mm_cmpeq_epi64(key4, ck1);
        const __m128i cmp2 = _mm_cmpeq_epi64(key4, ck2);
        const __m128i cmp3 = _mm_cmpeq_epi64(key4, ck3);
        const __m128i cmp4 = _mm_cmpeq_epi64(key4, ck4);
        const __m128i cmp5 = _mm_cmpeq_epi64(key4, ck5);
        const __m128i cmp6 = _mm_cmpeq_epi64(key4, ck6);
        const __m128i cmp7 = _mm_cmpeq_epi64(key4, ck7);

        const __m128i pack01 = _mm_packs_epi32(cmp0, cmp1);
        const __m128i pack23 = _mm_packs_epi32(cmp2, cmp3);
        const __m128i pack45 = _mm_packs_epi32(cmp4, cmp5);
        const __m128i pack67 = _mm_packs_epi32(cmp6, cmp7);

        const __m128i pack0123 = _mm_packs_epi16(pack01, pack23);
        const __m128i pack4567 = _mm_packs_epi16(pack45, pack67);

        const __m128i ck8 = _mm_load_si128((const __m128i*) & items[i + 16]);
        const __m128i ck9 = _mm_load_si128((const __m128i*) & items[i + 18]);
        const __m128i ckA = _mm_load_si128((const __m128i*) & items[i + 20]);
        const __m128i ckB = _mm_load_si128((const __m128i*) & items[i + 22]);
        const __m128i ckC = _mm_load_si128((const __m128i*) & items[i + 24]);
        const __m128i ckD = _mm_load_si128((const __m128i*) & items[i + 26]);
        const __m128i ckE = _mm_load_si128((const __m128i*) & items[i + 28]);
        const __m128i ckF = _mm_load_si128((const __m128i*) & items[i + 30]);

        const __m128i cmp8 = _mm_cmpeq_epi64(key4, ck8);
        const __m128i cmp9 = _mm_cmpeq_epi64(key4, ck9);
        const __m128i cmpA = _mm_cmpeq_epi64(key4, ckA);
        const __m128i cmpB = _mm_cmpeq_epi64(key4, ckB);
        const __m128i cmpC = _mm_cmpeq_epi64(key4, ckC);
        const __m128i cmpD = _mm_cmpeq_epi64(key4, ckD);
        const __m128i cmpE = _mm_cmpeq_epi64(key4, ckE);
        const __m128i cmpF = _mm_cmpeq_epi64(key4, ckF);

        const __m128i pack89 = _mm_packs_epi32(cmp8, cmp9);
        const __m128i packAB = _mm_packs_epi32(cmpA, cmpB);
        const __m128i packCD = _mm_packs_epi32(cmpC, cmpD);
        const __m128i packEF = _mm_packs_epi32(cmpE, cmpF);

        const __m128i pack89AB = _mm_packs_epi16(pack89, packAB);
        const __m128i packCDEF = _mm_packs_epi16(packCD, packEF);

        const uint32_t res01234567 = (uint32_t)_mm_movemask_epi8(pack0123) | ((uint32_t)_mm_movemask_epi8(pack4567) << 16);
        const uint32_t res89ABCDEF = (uint32_t)_mm_movemask_epi8(pack89AB) | ((uint32_t)_mm_movemask_epi8(packCDEF) << 16);

        res = res01234567 | ((uint64_t)res89ABCDEF << 32);
        if (res > 0) {
            break;
        }
    }

    if (res > 0)
    {
        DWORD bitIndex = 0;
        if (BitScanReverse64(&bitIndex, res))
        {
            findIndex = i + bitIndex / 2;
            assert(findIndex >= 0 && findIndex < (int32_t)itemsCount);
            assert(items[findIndex] == item);
            return findIndex;
        }
    }

    return -1;
}
