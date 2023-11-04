//--------------------------------------------------------------------------------------
// File: DirectXTexHelper.cpp
//
// DirectXTex helper functions for JPEG/PNG sources
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------
#include "DirectXTexP.h"

#include <cstdio>
#include <system_error>
#include <filesystem>
#include <memory>

namespace DirectX
{
    using std::filesystem::path;

    std::unique_ptr<FILE, int(*)(FILE*)> OpenFILE(const path& p) noexcept(false);
    std::unique_ptr<FILE, int(*)(FILE*)> CreateFILE(const path& p) noexcept(false);

#if defined(_WIN32)
    std::unique_ptr<FILE, int(*)(FILE*)> OpenFILE(const path& p) noexcept(false)
    {
        const std::wstring fpath = p.generic_wstring();
        FILE* fp = nullptr;
        if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"rb"); ec)
            throw std::system_error{ ec, std::system_category(), "_wfopen_s" };
        return { fp, &fclose };
    }
    std::unique_ptr<FILE, int(*)(FILE*)> CreateFILE(const path& p) noexcept(false)
    {
        const std::wstring fpath = p.generic_wstring();
        FILE* fp = nullptr;
        if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"w+b"); ec)
            throw std::system_error{ ec, std::system_category(), "_wfopen_s" };
        return { fp, &fclose };
    }
#else
    std::unique_ptr<FILE, int(*)(FILE*)> OpenFILE(const path& p) noexcept(false)
    {
        const std::string fpath = p.generic_string();
        FILE* fp = fopen(fpath.c_str(), "rb");
        if (fp == nullptr)
            throw std::system_error{ errno, std::system_category(), "fopen" };
        return { fp, &fclose };
    }
    std::unique_ptr<FILE, int(*)(FILE*)> CreateFILE(const path& p) noexcept(false)
    {
        const std::string fpath = p.generic_string();
        FILE* fp = fopen(fpath.c_str(), "w+b");
        if (fp == nullptr)
            throw std::system_error{ errno, std::system_category(), "fopen" };
        return { fp, &fclose };
    }
#endif
}
