//-------------------------------------------------------------------------------------
// scoped.h
//  
// Utility header with helper classes for exception-safe handling of resources
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#include <assert.h>
#include <memory>
#include <malloc.h>

//---------------------------------------------------------------------------------
struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

typedef std::unique_ptr<float, aligned_deleter> ScopedAlignedArrayFloat;

#ifdef USE_XNAMATH
typedef std::unique_ptr<XMVECTOR, aligned_deleter> ScopedAlignedArrayXMVECTOR;
#else
typedef std::unique_ptr<DirectX::XMVECTOR, aligned_deleter> ScopedAlignedArrayXMVECTOR;
#endif

//---------------------------------------------------------------------------------
struct handle_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

typedef public std::unique_ptr<void, handle_closer> ScopedHandle;

inline HANDLE safe_handle( HANDLE h ) { return (h == INVALID_HANDLE_VALUE) ? 0 : h; }


//---------------------------------------------------------------------------------
#if defined(_MSC_VER) && (_MSC_VER >= 1610)

#include <wrl.h>

template<class T> class ScopedObject : public Microsoft::WRL::ComPtr<T>
{
public:
    ScopedObject() : Microsoft::WRL::ComPtr<T>() {}
    ScopedObject( T *p ) : Microsoft::WRL::ComPtr<T>(p) {}
    ScopedObject( const ScopedObject& other ) : Microsoft::WRL::ComPtr( other ) {}
};

#else

template<class T> class ScopedObject
{
public:
    ScopedObject() : _pointer(nullptr) {}
    ScopedObject( T *p ) : _pointer(p) { if (_pointer) { _pointer->AddRef(); } }
    ScopedObject( const ScopedObject& other ) : _pointer(other._pointer) { if (_pointer) { _pointer->AddRef(); } }

    ~ScopedObject()
    {
        if ( _pointer )
        {
            _pointer->Release();
            _pointer = nullptr;
        }
    }

    operator bool() const { return (_pointer != nullptr); }

    ScopedObject& operator= (_In_opt_ T* other)
    {
        if ( _pointer != other )
        {
            if ( _pointer) { _pointer->Release(); }
            _pointer = other;
            if ( other ) { other->AddRef(); };
        }
        return *this;
    }

    ScopedObject& operator= (const ScopedObject& other)
    {
        if ( _pointer != other._pointer )
        {
            if ( _pointer) { _pointer->Release(); }
            _pointer = other._pointer;
            if ( other._pointer ) { other._pointer->AddRef(); }
        }
        return *this;
    }

    T& operator*() { return *_pointer; }

    T* operator->() const { return _pointer; }

    T** operator&() { return &_pointer; }

    void Reset() { if ( _pointer ) { _pointer->Release(); _pointer = nullptr; } }

    T* Get() const { return _pointer; }
    T** GetAddressOf() { return &_pointer; }

    T** ReleaseAndGetAddressOf() { if ( _pointer ) { _pointer->Release(); _pointer = nullptr; } return &_pointer; }

    template<typename U>
    HRESULT As(_Inout_ U* p) { return _pointer->QueryInterface( _uuidof(U), reinterpret_cast<void**>( p ) ); }

    template<typename U>
    HRESULT As(_Out_ ScopedObject<U>* p ) { return _pointer->QueryInterface( _uuidof(U), reinterpret_cast<void**>( p->ReleaseAndGetAddressOf() ) ); }

private:
    T* _pointer;
};

#endif
