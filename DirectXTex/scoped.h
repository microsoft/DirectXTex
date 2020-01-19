//-------------------------------------------------------------------------------------
// scoped.h
//  
// Utility header with helper classes for exception-safe handling of resources
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#pragma once

#include <assert.h>
#include <memory>
#include <malloc.h>

//---------------------------------------------------------------------------------
struct aligned_deleter { void operator()(void* p) noexcept { _aligned_free(p); } };

using ScopedAlignedArrayFloat = std::unique_ptr<float[], aligned_deleter>;

using ScopedAlignedArrayXMVECTOR = std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter>;

//---------------------------------------------------------------------------------
struct handle_closer { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

using ScopedHandle = std::unique_ptr<void, handle_closer>;

inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

//---------------------------------------------------------------------------------
struct find_closer { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

using ScopedFindHandle = std::unique_ptr<void, find_closer>;

//---------------------------------------------------------------------------------
class auto_delete_file
{
public:
    auto_delete_file(HANDLE hFile) noexcept : m_handle(hFile) {}

    auto_delete_file(const auto_delete_file&) = delete;
    auto_delete_file& operator=(const auto_delete_file&) = delete;

    ~auto_delete_file()
    {
        if (m_handle)
        {
            FILE_DISPOSITION_INFO info = {};
            info.DeleteFile = TRUE;
            (void)SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, sizeof(info));
        }
    }

    void clear() noexcept { m_handle = nullptr; }

private:
    HANDLE m_handle;
};
