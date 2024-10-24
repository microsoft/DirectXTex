//-------------------------------------------------------------------------------------
// DirectXTexWIC.cpp
//
// DirectX Texture Library - WIC-based file reader/writer
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

using namespace DirectX;
using namespace DirectX::Internal;
using Microsoft::WRL::ComPtr;

namespace
{
    //-------------------------------------------------------------------------------------
    // WIC Pixel Format nearest conversion table
    //-------------------------------------------------------------------------------------
    struct WICConvert
    {
        const GUID&     source;
        const GUID&     target;
        TEX_ALPHA_MODE  alphaMode;

        constexpr WICConvert(const GUID& src, const GUID& tgt, TEX_ALPHA_MODE mode) noexcept :
            source(src),
            target(tgt),
            alphaMode(mode) {}
    };

    constexpr WICConvert g_WICConvert[] =
    {
        // Directly support the formats listed in XnaTexUtil::g_WICFormats, so no conversion required
        // Note target GUID in this conversion table must be one of those directly supported formats.

        { GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8_UNORM
        { GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16_FLOAT
        { GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R32_FLOAT

        { GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_B5G5R5A1_UNORM
        { GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R10G10B10A2_UNORM

        { GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16G16B16A16_UNORM

        { GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_UNKNOWN  }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_UNKNOWN  }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_FLOAT

        { GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R32G32B32A32_FLOAT

        { GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16G16B16A16_UNORM

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        { GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA, TEX_ALPHA_MODE_OPAQUE }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf, TEX_ALPHA_MODE_UNKNOWN }, // DXGI_FORMAT_R16G16B16A16_FLOAT
    #endif

        // We don't support n-channel formats
    };

    //-------------------------------------------------------------------------------------
    // Returns the DXGI format and optionally the WIC pixel GUID to convert to
    //-------------------------------------------------------------------------------------
    DXGI_FORMAT DetermineFormat(
        _In_ const WICPixelFormatGUID& pixelFormat,
        WIC_FLAGS flags,
        bool iswic2,
        _Out_opt_ WICPixelFormatGUID* pConvert,
        _Out_ TEX_ALPHA_MODE* alphaMode) noexcept
    {
        if (pConvert)
            memset(pConvert, 0, sizeof(WICPixelFormatGUID));

        *alphaMode = TEX_ALPHA_MODE_UNKNOWN;

        DXGI_FORMAT format = WICToDXGI(pixelFormat);

        if (format == DXGI_FORMAT_UNKNOWN)
        {
            if (memcmp(&GUID_WICPixelFormat96bppRGBFixedPoint, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
            {
            #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
                if (iswic2)
                {
                    if (pConvert)
                        memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat96bppRGBFloat, sizeof(GUID));
                    format = DXGI_FORMAT_R32G32B32_FLOAT;
                }
                else
                #else
                UNREFERENCED_PARAMETER(iswic2);
            #endif
                {
                    if (pConvert)
                        memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat128bppRGBAFloat, sizeof(GUID));
                    format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                    *alphaMode = TEX_ALPHA_MODE_OPAQUE;
                }
            }
            else
            {
                for (size_t i = 0; i < std::size(g_WICConvert); ++i)
                {
                    if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
                    {
                        if (pConvert)
                            memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &g_WICConvert[i].target, sizeof(GUID));

                        format = WICToDXGI(g_WICConvert[i].target);
                        assert(format != DXGI_FORMAT_UNKNOWN);
                        *alphaMode = g_WICConvert[i].alphaMode;
                        break;
                    }
                }
            }
        }

        // Handle special cases based on flags
        switch (format)
        {
        case DXGI_FORMAT_B8G8R8A8_UNORM:    // BGRA
        case DXGI_FORMAT_B8G8R8X8_UNORM:    // BGRX
            if (flags & WIC_FLAGS_FORCE_RGB)
            {
                format = DXGI_FORMAT_R8G8B8A8_UNORM;
                if (pConvert)
                    memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            }
            break;

        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
            if (flags & WIC_FLAGS_NO_X2_BIAS)
            {
                format = DXGI_FORMAT_R10G10B10A2_UNORM;
                if (pConvert)
                    memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA1010102, sizeof(GUID));
            }
            break;

        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            if (flags & WIC_FLAGS_NO_16BPP)
            {
                format = DXGI_FORMAT_R8G8B8A8_UNORM;
                if (pConvert)
                    memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            }
            break;

        case DXGI_FORMAT_R1_UNORM:
            if (!(flags & WIC_FLAGS_ALLOW_MONO))
            {
                // By default we want to promote a black & white to gresycale since R1 is not a generally supported D3D format
                format = DXGI_FORMAT_R8_UNORM;
                if (pConvert)
                    memcpy_s(pConvert, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat8bppGray, sizeof(GUID));
            }
            break;

        default:
            break;
        }

        return format;
    }


    //-------------------------------------------------------------------------------------
    // IStream over a Blob for WIC in-memory write functions
    //-------------------------------------------------------------------------------------
    class MemoryStreamOnBlob : public IStream
    {
        MemoryStreamOnBlob(Blob& blob) noexcept :
            mBlob(blob),
            m_streamPosition(0),
            m_streamEOF(0),
            mRefCount(1)
        {
            assert(mBlob.GetConstBufferPointer() && mBlob.GetBufferSize() > 0);
        }

    public:
        virtual ~MemoryStreamOnBlob() = default;

        MemoryStreamOnBlob(MemoryStreamOnBlob&&) = delete;
        MemoryStreamOnBlob& operator= (MemoryStreamOnBlob&&) = delete;

        MemoryStreamOnBlob(MemoryStreamOnBlob const&) = delete;
        MemoryStreamOnBlob& operator= (MemoryStreamOnBlob const&) = delete;

        // IUnknown
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
        {
            if (iid == __uuidof(IUnknown)
                || iid == __uuidof(IStream)
                || iid == __uuidof(ISequentialStream))
            {
                *ppvObject = static_cast<IStream*>(this);
                AddRef();
                return S_OK;
            }
            else
                return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return InterlockedIncrement(&mRefCount);
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG res = InterlockedDecrement(&mRefCount);
            if (res == 0)
            {
                delete this;
            }
            return res;
        }

        // ISequentialStream
        HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override
        {
            size_t maxRead = m_streamEOF - m_streamPosition;
            auto ptr = mBlob.GetBufferPointer();
            if (cb > maxRead)
            {
                const uint64_t pos = uint64_t(m_streamPosition) + uint64_t(maxRead);
                if (pos > UINT32_MAX)
                    return HRESULT_E_ARITHMETIC_OVERFLOW;

                memcpy(pv, &ptr[m_streamPosition], maxRead);

                m_streamPosition = static_cast<size_t>(pos);

                if (pcbRead)
                {
                    *pcbRead = static_cast<ULONG>(maxRead);
                }
                return E_BOUNDS;
            }
            else
            {
                const uint64_t pos = uint64_t(m_streamPosition) + uint64_t(cb);
                if (pos > UINT32_MAX)
                    return HRESULT_E_ARITHMETIC_OVERFLOW;

                memcpy(pv, &ptr[m_streamPosition], cb);

                m_streamPosition = static_cast<size_t>(pos);

                if (pcbRead)
                {
                    *pcbRead = cb;
                }
                return S_OK;
            }
        }

        HRESULT STDMETHODCALLTYPE Write(void const* pv, ULONG cb, ULONG* pcbWritten) override
        {
            const size_t blobSize = mBlob.GetBufferSize();
            const size_t spaceAvailable = blobSize - m_streamPosition;
            size_t growAmount = cb;

            if (spaceAvailable > 0)
            {
                if (spaceAvailable >= growAmount)
                {
                    growAmount = 0;
                }
                else
                {
                    growAmount -= spaceAvailable;
                }
            }

            if (growAmount > 0)
            {
                uint64_t newSize = uint64_t(blobSize);
                const uint64_t targetSize = uint64_t(blobSize) + growAmount;
                HRESULT hr = ComputeGrowSize(newSize, targetSize);
                if (FAILED(hr))
                    return hr;

                hr = mBlob.Resize(static_cast<size_t>(newSize));
                if (FAILED(hr))
                    return hr;
            }

            const uint64_t pos = uint64_t(m_streamPosition) + uint64_t(cb);
            if (pos > UINT32_MAX)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            auto ptr = mBlob.GetBufferPointer();
            memcpy(&ptr[m_streamPosition], pv, cb);

            m_streamPosition = static_cast<size_t>(pos);
            m_streamEOF = std::max(m_streamEOF, m_streamPosition);

            if (pcbWritten)
            {
                *pcbWritten = cb;
            }
            return S_OK;
        }

        // IStream
        HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER size) override
        {
            if (size.HighPart > 0)
                return E_OUTOFMEMORY;

            const size_t blobSize = mBlob.GetBufferSize();

            if (blobSize >= size.LowPart)
            {
                auto ptr = mBlob.GetBufferPointer();
                if (m_streamEOF < size.LowPart)
                {
                    memset(&ptr[m_streamEOF], 0, size.LowPart - m_streamEOF);
                }

                m_streamEOF = static_cast<size_t>(size.LowPart);
            }
            else
            {
                uint64_t newSize = uint64_t(blobSize);
                const uint64_t targetSize = uint64_t(size.QuadPart);
                HRESULT hr = ComputeGrowSize(newSize, targetSize);
                if (FAILED(hr))
                    return hr;

                hr = mBlob.Resize(static_cast<size_t>(newSize));
                if (FAILED(hr))
                    return hr;

                auto ptr = mBlob.GetBufferPointer();
                if (m_streamEOF < size.LowPart)
                {
                    memset(&ptr[m_streamEOF], 0, size.LowPart - m_streamEOF);
                }

                m_streamEOF = static_cast<size_t>(size.LowPart);
            }

            if (m_streamPosition > m_streamEOF)
            {
                m_streamPosition = m_streamEOF;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Commit(DWORD) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Revert() override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Clone(IStream**) override
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, ULARGE_INTEGER* lpNewFilePointer) override
        {
            LONGLONG newPosition = 0;

            switch (dwOrigin)
            {
            case STREAM_SEEK_SET:
                newPosition = liDistanceToMove.QuadPart;
                break;

            case STREAM_SEEK_CUR:
                newPosition = static_cast<LONGLONG>(m_streamPosition) + liDistanceToMove.QuadPart;
                break;

            case STREAM_SEEK_END:
                newPosition = static_cast<LONGLONG>(m_streamEOF) + liDistanceToMove.QuadPart;
                break;

            default:
                return STG_E_INVALIDFUNCTION;
            }

            HRESULT result = S_OK;

            if (newPosition > static_cast<LONGLONG>(m_streamEOF))
            {
                m_streamPosition = m_streamEOF;
                result = E_BOUNDS;
            }
            else if (newPosition < 0)
            {
                m_streamPosition = 0;
                result = E_BOUNDS;
            }
            else
            {
                m_streamPosition = static_cast<size_t>(newPosition);
            }

            if (lpNewFilePointer)
            {
                lpNewFilePointer->QuadPart = static_cast<ULONGLONG>(m_streamPosition);
            }

            return result;
        }

        HRESULT STDMETHODCALLTYPE Stat(STATSTG* pStatstg, DWORD) override
        {
            if (!pStatstg)
                return E_INVALIDARG;
            pStatstg->cbSize.QuadPart = static_cast<ULONGLONG>(m_streamEOF);
            return S_OK;
        }

        HRESULT Finialize() noexcept
        {
            if (mRefCount > 1)
                return E_FAIL;

            return mBlob.Trim(m_streamEOF);
        }

        static HRESULT CreateMemoryStream(_Outptr_ MemoryStreamOnBlob** stream, Blob& blob) noexcept
        {
            if (!stream)
                return E_INVALIDARG;

            *stream = nullptr;

            auto ptr = new (std::nothrow) MemoryStreamOnBlob(blob);
            if (!ptr)
                return E_OUTOFMEMORY;

            *stream = ptr;

            return S_OK;
        }

    private:
        Blob& mBlob;
        size_t m_streamPosition;
        size_t m_streamEOF;
        ULONG mRefCount;

        static HRESULT ComputeGrowSize(uint64_t& newSize, const uint64_t targetSize) noexcept
        {
            // We grow by doubling until we hit 256MB, then we add 16MB at a time.
            while (newSize < targetSize)
            {
                if (newSize < (256 * 1024 * 1024))
                {
                    newSize <<= 1;
                }
                else
                {
                    newSize += 16 * 1024 * 1024;
                }
                if (newSize > UINT32_MAX)
                    return E_OUTOFMEMORY;
            }

            return S_OK;
        }
    };

    //-------------------------------------------------------------------------------------
    // Determines metadata for image
    //-------------------------------------------------------------------------------------
    HRESULT DecodeMetadata(
        WIC_FLAGS flags,
        bool iswic2,
        _In_ IWICBitmapDecoder *decoder,
        _In_ IWICBitmapFrameDecode *frame,
        _Out_ TexMetadata& metadata,
        _Out_opt_ WICPixelFormatGUID* pConvert,
        _In_ std::function<void(IWICMetadataQueryReader*)> getMQR)
    {
        if (!decoder || !frame)
            return E_POINTER;

        memset(&metadata, 0, sizeof(TexMetadata));
        metadata.depth = 1;
        metadata.mipLevels = 1;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;

        UINT w, h;
        HRESULT hr = frame->GetSize(&w, &h);
        if (FAILED(hr))
            return hr;

        metadata.width = w;
        metadata.height = h;

        if (flags & WIC_FLAGS_ALL_FRAMES)
        {
            UINT fcount;
            hr = decoder->GetFrameCount(&fcount);
            if (FAILED(hr))
                return hr;

            metadata.arraySize = fcount;
        }
        else
            metadata.arraySize = 1;

        WICPixelFormatGUID pixelFormat;
        hr = frame->GetPixelFormat(&pixelFormat);
        if (FAILED(hr))
            return hr;

        TEX_ALPHA_MODE alphaMode;
        metadata.format = DetermineFormat(pixelFormat, flags, iswic2, pConvert, &alphaMode);
        if (metadata.format == DXGI_FORMAT_UNKNOWN)
            return HRESULT_E_NOT_SUPPORTED;

        metadata.SetAlphaMode(alphaMode);

        if (!(flags & WIC_FLAGS_IGNORE_SRGB))
        {
            GUID containerFormat;
            hr = decoder->GetContainerFormat(&containerFormat);
            if (FAILED(hr))
                return hr;

            ComPtr<IWICMetadataQueryReader> metareader;
            hr = frame->GetMetadataQueryReader(metareader.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                // Check for sRGB colorspace metadata
                bool sRGB = false;

                PROPVARIANT value;
                PropVariantInit(&value);

                // Check for colorspace chunks
                if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
                {
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) && value.vt == VT_UI1)
                    {
                        sRGB = true;
                    }
                    else if (SUCCEEDED(metareader->GetMetadataByName(L"/gAMA/ImageGamma", &value)) && value.vt == VT_UI4)
                    {
                        sRGB = (value.uintVal == 45455);
                    }
                    else
                    {
                        sRGB = (flags & WIC_FLAGS_DEFAULT_SRGB) != 0;
                    }
                }
            #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
                else if (memcmp(&containerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID)) == 0)
                {
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                    {
                        sRGB = (value.uiVal == 1);
                    }
                    else
                    {
                        sRGB = (flags & WIC_FLAGS_DEFAULT_SRGB) != 0;
                    }
                }
                else if (memcmp(&containerFormat, &GUID_ContainerFormatTiff, sizeof(GUID)) == 0)
                {
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                    {
                        sRGB = (value.uiVal == 1);
                    }
                    else
                    {
                        sRGB = (flags & WIC_FLAGS_DEFAULT_SRGB) != 0;
                    }
                }
            #else
                else if (SUCCEEDED(metareader->GetMetadataByName(L"System.Image.ColorSpace", &value)) && value.vt == VT_UI2)
                {
                    sRGB = (value.uiVal == 1);
                }
                else
                {
                    sRGB = (flags & WIC_FLAGS_DEFAULT_SRGB) != 0;
                }
            #endif

                std::ignore = PropVariantClear(&value);

                if (sRGB)
                    metadata.format = MakeSRGB(metadata.format);
            }
            else if (hr == WINCODEC_ERR_UNSUPPORTEDOPERATION)
            {
                // Some formats just don't support metadata (BMP, ICO, etc.), so ignore this failure
                hr = S_OK;
            }
        }

        if (getMQR)
        {
            ComPtr<IWICMetadataQueryReader> metareader;
            if (SUCCEEDED(frame->GetMetadataQueryReader(metareader.GetAddressOf())))
            {
                getMQR(metareader.Get());
            }
        }

        return hr;
    }


    //-------------------------------------------------------------------------------------
    // Decodes a single frame
    //-------------------------------------------------------------------------------------
    HRESULT DecodeSingleFrame(
        WIC_FLAGS flags,
        const TexMetadata& metadata,
        const WICPixelFormatGUID& convertGUID,
        _In_ IWICBitmapFrameDecode *frame,
        _Inout_ ScratchImage& image)
    {
        if (!frame)
            return E_POINTER;

        HRESULT hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image *img = image.GetImage(0, 0, 0);
        if (!img)
            return E_POINTER;

        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        if (img->rowPitch > UINT32_MAX || img->slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        if (memcmp(&convertGUID, &GUID_NULL, sizeof(GUID)) == 0)
        {
            hr = frame->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
            if (FAILED(hr))
                return hr;
        }
        else
        {
            ComPtr<IWICFormatConverter> FC;
            hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
            if (FAILED(hr))
                return hr;

            WICPixelFormatGUID pixelFormat;
            hr = frame->GetPixelFormat(&pixelFormat);
            if (FAILED(hr))
                return hr;

            BOOL canConvert = FALSE;
            hr = FC->CanConvert(pixelFormat, convertGUID, &canConvert);
            if (FAILED(hr) || !canConvert)
            {
                return E_UNEXPECTED;
            }

            hr = FC->Initialize(frame, convertGUID, GetWICDither(flags), nullptr,
                0, WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr))
                return hr;

            hr = FC->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
            if (FAILED(hr))
                return hr;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Decodes an image array, resizing/format converting as needed
    //-------------------------------------------------------------------------------------
    HRESULT DecodeMultiframe(
        WIC_FLAGS flags,
        const TexMetadata& metadata,
        _In_ IWICBitmapDecoder *decoder,
        _Inout_ ScratchImage& image)
    {
        if (!decoder)
            return E_POINTER;

        HRESULT hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, 1);
        if (FAILED(hr))
            return hr;

        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        WICPixelFormatGUID sourceGUID;
        if (!DXGIToWIC(metadata.format, sourceGUID))
            return E_FAIL;

        for (size_t index = 0; index < metadata.arraySize; ++index)
        {
            const Image* img = image.GetImage(0, index, 0);
            if (!img)
                return E_POINTER;

            if (img->rowPitch > UINT32_MAX || img->slicePitch > UINT32_MAX)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(static_cast<UINT>(index), frame.GetAddressOf());
            if (FAILED(hr))
                return hr;

            WICPixelFormatGUID pfGuid;
            hr = frame->GetPixelFormat(&pfGuid);
            if (FAILED(hr))
                return hr;

            UINT w, h;
            hr = frame->GetSize(&w, &h);
            if (FAILED(hr))
                return hr;

            if (w == metadata.width && h == metadata.height)
            {
                // This frame does not need resized
                if (memcmp(&pfGuid, &sourceGUID, sizeof(WICPixelFormatGUID)) == 0)
                {
                    hr = frame->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
                else
                {
                    ComPtr<IWICFormatConverter> FC;
                    hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
                    if (FAILED(hr))
                        return hr;

                    BOOL canConvert = FALSE;
                    hr = FC->CanConvert(pfGuid, sourceGUID, &canConvert);
                    if (FAILED(hr) || !canConvert)
                    {
                        return E_UNEXPECTED;
                    }

                    hr = FC->Initialize(frame.Get(), sourceGUID, GetWICDither(flags), nullptr,
                        0, WICBitmapPaletteTypeMedianCut);
                    if (FAILED(hr))
                        return hr;

                    hr = FC->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
            }
            else
            {
                // This frame needs resizing
                ComPtr<IWICBitmapScaler> scaler;
                hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                hr = scaler->Initialize(frame.Get(),
                    static_cast<UINT>(metadata.width), static_cast<UINT>(metadata.height),
                    GetWICInterp(flags));
                if (FAILED(hr))
                    return hr;

                WICPixelFormatGUID pfScaler;
                hr = scaler->GetPixelFormat(&pfScaler);
                if (FAILED(hr))
                    return hr;

                if (memcmp(&pfScaler, &sourceGUID, sizeof(WICPixelFormatGUID)) == 0)
                {
                    hr = scaler->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
                else
                {
                    // The WIC bitmap scaler is free to return a different pixel format than the source image, so here we
                    // convert it to our desired format
                    ComPtr<IWICFormatConverter> FC;
                    hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
                    if (FAILED(hr))
                        return hr;

                    BOOL canConvert = FALSE;
                    hr = FC->CanConvert(pfScaler, sourceGUID, &canConvert);
                    if (FAILED(hr) || !canConvert)
                    {
                        return E_UNEXPECTED;
                    }

                    hr = FC->Initialize(scaler.Get(), sourceGUID, GetWICDither(flags), nullptr,
                        0, WICBitmapPaletteTypeMedianCut);
                    if (FAILED(hr))
                        return hr;

                    hr = FC->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
            }
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Encodes image metadata
    //-------------------------------------------------------------------------------------
    HRESULT EncodeMetadata(
        WIC_FLAGS flags,
        _In_ IWICBitmapFrameEncode* frame,
        const GUID& containerFormat,
        DXGI_FORMAT format)
    {
        if (!frame)
            return E_POINTER;

        ComPtr<IWICMetadataQueryWriter> metawriter;
        HRESULT hr = frame->GetMetadataQueryWriter(metawriter.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            PROPVARIANT value;
            PropVariantInit(&value);

            const bool sRGB = ((flags & WIC_FLAGS_FORCE_LINEAR) == 0) && ((flags & WIC_FLAGS_FORCE_SRGB) != 0 || IsSRGB(format));

            value.vt = VT_LPSTR;
            value.pszVal = const_cast<char*>("DirectXTex");

            if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
            {
                // Set Software name
                std::ignore = metawriter->SetMetadataByName(L"/tEXt/{str=Software}", &value);

                // Set sRGB chunk
                if (sRGB)
                {
                    value.vt = VT_UI1;
                    value.bVal = 0;
                    std::ignore = metawriter->SetMetadataByName(L"/sRGB/RenderingIntent", &value);
                }
                else
                {
                    // add gAMA chunk with gamma 1.0
                    value.vt = VT_UI4;
                    value.uintVal = 100000; // gama value * 100,000 -- i.e. gamma 1.0
                    std::ignore = metawriter->SetMetadataByName(L"/gAMA/ImageGamma", &value);

                    // remove sRGB chunk which is added by default.
                    std::ignore = metawriter->RemoveMetadataByName(L"/sRGB/RenderingIntent");
                }
            }
        #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
            else if (memcmp(&containerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID)) == 0)
            {
                // Set Software name
                std::ignore = metawriter->SetMetadataByName(L"/app1/ifd/{ushort=305}", &value);

                if (sRGB)
                {
                    // Set EXIF Colorspace of sRGB
                    value.vt = VT_UI2;
                    value.uiVal = 1;
                    std::ignore = metawriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value);
                }
            }
            else if (memcmp(&containerFormat, &GUID_ContainerFormatTiff, sizeof(GUID)) == 0)
            {
                // Set Software name
                std::ignore = metawriter->SetMetadataByName(L"/ifd/{ushort=305}", &value);

                if (sRGB)
                {
                    // Set EXIF Colorspace of sRGB
                    value.vt = VT_UI2;
                    value.uiVal = 1;
                    std::ignore = metawriter->SetMetadataByName(L"/ifd/exif/{ushort=40961}", &value);
                }
            }
        #else
            else
            {
                // Set Software name
                std::ignore = metawriter->SetMetadataByName(L"System.ApplicationName", &value);

                if (sRGB)
                {
                    // Set EXIF Colorspace of sRGB
                    value.vt = VT_UI2;
                    value.uiVal = 1;
                    std::ignore = metawriter->SetMetadataByName(L"System.Image.ColorSpace", &value);
                }
            }
        #endif
        }
        else if (hr == WINCODEC_ERR_UNSUPPORTEDOPERATION)
        {
            // Some formats just don't support metadata (BMP, ICO, etc.), so ignore this failure
            hr = S_OK;
        }

        return hr;
    }


    //-------------------------------------------------------------------------------------
    // Encodes a single frame
    //-------------------------------------------------------------------------------------
    HRESULT EncodeImage(
        const Image& image,
        WIC_FLAGS flags,
        _In_ REFGUID containerFormat,
        _In_ IWICBitmapFrameEncode* frame,
        _In_opt_ IPropertyBag2* props,
        _In_opt_ const GUID* targetFormat)
    {
        if (!frame)
            return E_INVALIDARG;

        if (!image.pixels)
            return E_POINTER;

        WICPixelFormatGUID pfGuid;
        if (!DXGIToWIC(image.format, pfGuid))
            return HRESULT_E_NOT_SUPPORTED;

        HRESULT hr = frame->Initialize(props);
        if (FAILED(hr))
            return hr;

        if ((image.width > UINT32_MAX) || (image.height > UINT32_MAX))
            return E_INVALIDARG;

        if (image.rowPitch > UINT32_MAX || image.slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        hr = frame->SetSize(static_cast<UINT>(image.width), static_cast<UINT>(image.height));
        if (FAILED(hr))
            return hr;

        hr = frame->SetResolution(72, 72);
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID targetGuid = (targetFormat) ? (*targetFormat) : pfGuid;
        hr = frame->SetPixelFormat(&targetGuid);
        if (FAILED(hr))
            return hr;

        if (targetFormat && memcmp(targetFormat, &targetGuid, sizeof(WICPixelFormatGUID)) != 0)
        {
            // Requested output pixel format is not supported by the WIC codec
            return E_FAIL;
        }

        hr = EncodeMetadata(flags, frame, containerFormat, image.format);
        if (FAILED(hr))
            return hr;

        if (memcmp(&targetGuid, &pfGuid, sizeof(WICPixelFormatGUID)) != 0)
        {
            // Conversion required to write
            bool iswic2 = false;
            auto pWIC = GetWICFactory(iswic2);
            if (!pWIC)
                return E_NOINTERFACE;

            ComPtr<IWICBitmap> source;
            hr = pWIC->CreateBitmapFromMemory(static_cast<UINT>(image.width), static_cast<UINT>(image.height), pfGuid,
                static_cast<UINT>(image.rowPitch), static_cast<UINT>(image.slicePitch),
                image.pixels, source.GetAddressOf());
            if (FAILED(hr))
                return hr;

            ComPtr<IWICFormatConverter> FC;
            hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
            if (FAILED(hr))
                return hr;

            BOOL canConvert = FALSE;
            hr = FC->CanConvert(pfGuid, targetGuid, &canConvert);
            if (FAILED(hr) || !canConvert)
            {
                return E_UNEXPECTED;
            }

            hr = FC->Initialize(source.Get(), targetGuid, GetWICDither(flags), nullptr,
                0, WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr))
                return hr;

            WICRect rect = { 0, 0, static_cast<INT>(image.width), static_cast<INT>(image.height) };
            hr = frame->WriteSource(FC.Get(), &rect);
            if (FAILED(hr))
                return hr;
        }
        else
        {
            // No conversion required
            hr = frame->WritePixels(static_cast<UINT>(image.height), static_cast<UINT>(image.rowPitch), static_cast<UINT>(image.slicePitch),
                reinterpret_cast<uint8_t*>(image.pixels));
            if (FAILED(hr))
                return hr;
        }

        hr = frame->Commit();
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    HRESULT EncodeSingleFrame(
        const Image& image,
        WIC_FLAGS flags,
        _In_ REFGUID containerFormat,
        _Inout_ IStream* stream,
        _In_opt_ const GUID* targetFormat,
        _In_ std::function<void(IPropertyBag2*)> setCustomProps)
    {
        if (!stream)
            return E_INVALIDARG;

        // Initialize WIC
        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        ComPtr<IWICBitmapEncoder> encoder;
        HRESULT hr = pWIC->CreateEncoder(containerFormat, nullptr, encoder.GetAddressOf());
        if (FAILED(hr))
            return hr;

        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        if (FAILED(hr))
            return hr;

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> props;
        hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
        if (FAILED(hr))
            return hr;

        if (memcmp(&containerFormat, &GUID_ContainerFormatBmp, sizeof(WICPixelFormatGUID)) == 0 && iswic2)
        {
            // Opt-in to the WIC2 support for writing 32-bit Windows BMP files with an alpha channel
            PROPBAG2 option = {};
            option.pstrName = const_cast<wchar_t*>(L"EnableV5Header32bppBGRA");

            VARIANT varValue;
            varValue.vt = VT_BOOL;
            varValue.boolVal = VARIANT_TRUE;
            std::ignore = props->Write(1, &option, &varValue);
        }

        if (setCustomProps)
        {
            setCustomProps(props.Get());
        }

        hr = EncodeImage(image, flags, containerFormat, frame.Get(), props.Get(), targetFormat);
        if (FAILED(hr))
            return hr;

        hr = encoder->Commit();
        if (FAILED(hr))
            return hr;

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Encodes an image array
    //-------------------------------------------------------------------------------------
    HRESULT EncodeMultiframe(
        _In_reads_(nimages) const Image* images,
        size_t nimages,
        WIC_FLAGS flags,
        _In_ REFGUID containerFormat,
        _Inout_ IStream* stream,
        _In_opt_ const GUID* targetFormat,
        _In_ std::function<void(IPropertyBag2*)> setCustomProps)
    {
        if (!stream || nimages < 2)
            return E_INVALIDARG;

        if (!images)
            return E_POINTER;

        // Initialize WIC
        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        ComPtr<IWICBitmapEncoder> encoder;
        HRESULT hr = pWIC->CreateEncoder(containerFormat, nullptr, encoder.GetAddressOf());
        if (FAILED(hr))
            return hr;

        ComPtr<IWICBitmapEncoderInfo> einfo;
        hr = encoder->GetEncoderInfo(einfo.GetAddressOf());
        if (FAILED(hr))
            return hr;

        BOOL mframe = FALSE;
        hr = einfo->DoesSupportMultiframe(&mframe);
        if (FAILED(hr))
            return hr;

        if (!mframe)
            return HRESULT_E_NOT_SUPPORTED;

        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        if (FAILED(hr))
            return hr;

        for (size_t index = 0; index < nimages; ++index)
        {
            ComPtr<IWICBitmapFrameEncode> frame;
            ComPtr<IPropertyBag2> props;
            hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
            if (FAILED(hr))
                return hr;

            if (setCustomProps)
            {
                setCustomProps(props.Get());
            }

            hr = EncodeImage(images[index], flags, containerFormat, frame.Get(), props.Get(), targetFormat);
            if (FAILED(hr))
                return hr;
        }

        hr = encoder->Commit();
        if (FAILED(hr))
            return hr;

        return S_OK;
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from WIC-supported file in memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromWICMemory(
    const uint8_t* pSource,
    size_t size,
    WIC_FLAGS flags,
    TexMetadata& metadata,
    std::function<void(IWICMetadataQueryReader*)> getMQR)
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    if (size > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<uint8_t*>(pSource)),
        static_cast<UINT>(size));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    // Get metadata
    hr = DecodeMetadata(flags, iswic2, decoder.Get(), frame.Get(), metadata, nullptr, getMQR);
    if (FAILED(hr))
        return hr;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Obtain metadata from WIC-supported file on disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromWICFile(
    const wchar_t* szFile,
    WIC_FLAGS flags,
    TexMetadata& metadata,
    std::function<void(IWICMetadataQueryReader*)> getMQR)
{
    if (!szFile)
        return E_INVALIDARG;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(szFile, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    // Get metadata
    hr = DecodeMetadata(flags, iswic2, decoder.Get(), frame.Get(), metadata, nullptr, getMQR);
    if (FAILED(hr))
        return hr;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a WIC-supported file in memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromWICMemory(
    const uint8_t* pSource,
    size_t size,
    WIC_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image,
    std::function<void(IWICMetadataQueryReader*)> getMQR)
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    if (size > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    image.Release();

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(static_cast<uint8_t*>(const_cast<uint8_t*>(pSource)), static_cast<DWORD>(size));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    // Get metadata
    TexMetadata mdata = {};
    WICPixelFormatGUID convertGUID = {};
    hr = DecodeMetadata(flags, iswic2, decoder.Get(), frame.Get(), mdata, &convertGUID, getMQR);
    if (FAILED(hr))
        return hr;

    if ((mdata.arraySize > 1) && (flags & WIC_FLAGS_ALL_FRAMES))
    {
        hr = DecodeMultiframe(flags, mdata, decoder.Get(), image);
    }
    else
    {
        hr = DecodeSingleFrame(flags, mdata, convertGUID, frame.Get(), image);
    }

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a WIC-supported file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromWICFile(
    const wchar_t* szFile,
    WIC_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image,
    std::function<void(IWICMetadataQueryReader*)> getMQR)
{
    if (!szFile)
        return E_INVALIDARG;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    image.Release();

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(szFile, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    // Get metadata
    TexMetadata mdata = {};
    WICPixelFormatGUID convertGUID = {};
    hr = DecodeMetadata(flags, iswic2, decoder.Get(), frame.Get(), mdata, &convertGUID, getMQR);
    if (FAILED(hr))
        return hr;

    if ((mdata.arraySize > 1) && (flags & WIC_FLAGS_ALL_FRAMES))
    {
        hr = DecodeMultiframe(flags, mdata, decoder.Get(), image);
    }
    else
    {
        hr = DecodeSingleFrame(flags, mdata, convertGUID, frame.Get(), image);
    }

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a WIC-supported file to memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToWICMemory(
    const Image& image,
    WIC_FLAGS flags,
    REFGUID containerFormat,
    Blob& blob,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps)
{
    if (!image.pixels)
        return E_POINTER;

    HRESULT hr = blob.Initialize(65535u);
    if (FAILED(hr))
        return hr;

    ComPtr<MemoryStreamOnBlob> stream;
    hr = MemoryStreamOnBlob::CreateMemoryStream(&stream, blob);
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    hr = EncodeSingleFrame(image, flags, containerFormat, stream.Get(), targetFormat, setCustomProps);
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    hr = stream->Finialize();
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    stream.Reset();

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::SaveToWICMemory(
    const Image* images,
    size_t nimages,
    WIC_FLAGS flags,
    REFGUID containerFormat,
    Blob& blob,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps)
{
    if (!images || nimages == 0)
        return E_INVALIDARG;

    HRESULT hr = blob.Initialize(65535u);
    if (FAILED(hr))
        return hr;

    ComPtr<MemoryStreamOnBlob> stream;
    hr = MemoryStreamOnBlob::CreateMemoryStream(&stream, blob);
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    if (nimages > 1)
        hr = EncodeMultiframe(images, nimages, flags, containerFormat, stream.Get(), targetFormat, setCustomProps);
    else
        hr = EncodeSingleFrame(images[0], flags, containerFormat, stream.Get(), targetFormat, setCustomProps);

    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    hr = stream->Finialize();
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    stream.Reset();

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a WIC-supported file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToWICFile(
    const Image& image,
    WIC_FLAGS flags,
    REFGUID containerFormat,
    const wchar_t* szFile,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps)
{
    if (!szFile)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(szFile, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    hr = EncodeSingleFrame(image, flags, containerFormat, stream.Get(), targetFormat, setCustomProps);
    if (FAILED(hr))
    {
        stream.Reset();
        std::ignore = DeleteFileW(szFile);
        return hr;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::SaveToWICFile(
    const Image* images,
    size_t nimages,
    WIC_FLAGS flags,
    REFGUID containerFormat,
    const wchar_t* szFile,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps)
{
    if (!szFile || !images || nimages == 0)
        return E_INVALIDARG;

    bool iswic2 = false;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(szFile, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    if (nimages > 1)
        hr = EncodeMultiframe(images, nimages, flags, containerFormat, stream.Get(), targetFormat, setCustomProps);
    else
        hr = EncodeSingleFrame(images[0], flags, containerFormat, stream.Get(), targetFormat, setCustomProps);

    if (FAILED(hr))
    {
        stream.Reset();
        std::ignore = DeleteFileW(szFile);
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Adapters for /Zc:wchar_t- clients

#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

namespace DirectX
{
    HRESULT __cdecl GetMetadataFromWICFile(
        _In_z_ const __wchar_t* szFile,
        _In_ WIC_FLAGS flags,
        _Out_ TexMetadata& metadata,
        _In_ std::function<void __cdecl(IWICMetadataQueryReader*)> getMQR)
    {
        return GetMetadataFromWICFile(reinterpret_cast<const unsigned short*>(szFile), flags, metadata, getMQR);
    }

    HRESULT __cdecl LoadFromWICFile(
        _In_z_ const __wchar_t* szFile,
        _In_ WIC_FLAGS flags,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image,
        _In_ std::function<void __cdecl(IWICMetadataQueryReader*)> getMQR)
    {
        return LoadFromWICFile(reinterpret_cast<const unsigned short*>(szFile), flags, metadata, image, getMQR);
    }

    HRESULT __cdecl SaveToWICFile(
        _In_ const Image& image,
        _In_ WIC_FLAGS flags,
        _In_ REFGUID guidContainerFormat,
        _In_z_ const __wchar_t* szFile,
        _In_opt_ const GUID* targetFormat,
        _In_ std::function<void __cdecl(IPropertyBag2*)> setCustomProps)
    {
        return SaveToWICFile(image, flags, guidContainerFormat, reinterpret_cast<const unsigned short*>(szFile), targetFormat, setCustomProps);
    }

    HRESULT __cdecl SaveToWICFile(
        _In_count_(nimages) const Image* images,
        _In_ size_t nimages,
        _In_ WIC_FLAGS flags,
        _In_ REFGUID guidContainerFormat,
        _In_z_ const __wchar_t* szFile,
        _In_opt_ const GUID* targetFormat,
        _In_ std::function<void __cdecl(IPropertyBag2*)> setCustomProps)
    {
        return SaveToWICFile(images, nimages, flags, guidContainerFormat, reinterpret_cast<const unsigned short*>(szFile), targetFormat, setCustomProps);
    }
}

#endif // !_NATIVE_WCHAR_T_DEFINED
