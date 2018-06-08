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
struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

typedef std::unique_ptr<float[], aligned_deleter> ScopedAlignedArrayFloat;

typedef std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter> ScopedAlignedArrayXMVECTOR;

//---------------------------------------------------------------------------------
struct handle_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

typedef std::unique_ptr<void, handle_closer> ScopedHandle;

inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

//---------------------------------------------------------------------------------
struct find_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

typedef std::unique_ptr<void, find_closer> ScopedFindHandle;

//---------------------------------------------------------------------------------
class auto_delete_file
{
public:
    auto_delete_file(HANDLE hFile) : m_handle(hFile) {}

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

    void clear() { m_handle = nullptr; }

private:
    HANDLE m_handle;
};
