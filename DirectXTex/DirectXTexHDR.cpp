//-------------------------------------------------------------------------------------
// DirectXTexHDR.cpp
//
// DirectX Texture Library - Radiance HDR (RGBE) file format reader/writer
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"
//
// In theory HDR (RGBE) Radiance files can have any of the following data orientations
//
//      +X width +Y height
//      +X width -Y height
//      -X width +Y height
//      -X width -Y height
//      +Y height +X width
//      -Y height +X width
//      +Y height -X width
//      -Y height -X width
//
// All HDR files we've encountered are always written as "-Y height +X width", so
// we support only that one as that's what other Radiance parsing code does as well.
//

//Uncomment to disable the use of adapative RLE encoding when writing an HDR. Used for testing only.
//#define DISABLE_COMPRESS

//Uncomment to use "old colors" standard RLE encoding when writing an HDR. Used for testing only.
//#define WRITE_OLD_COLORS

using namespace DirectX;

#ifndef _WIN32
#include <cstdarg>

#define strncpy_s strncpy
#define sscanf_s sscanf
#endif

namespace
{
    const char g_Signature[] = "#?RADIANCE";
        // This is the official header signature for the .HDR (RGBE) file format.

    const char g_AltSignature[] = "#?RGBE";
        // This is a common variant header signature that is otherwise exactly the same format.

    const char g_Format[] = "FORMAT=";
    const char g_Exposure[] = "EXPOSURE=";

    const char g_sRGBE[] = "32-bit_rle_rgbe";
    const char g_sXYZE[] = "32-bit_rle_xyze";

    const char g_Header[] =
        "#?RADIANCE\n"\
        "FORMAT=32-bit_rle_rgbe\n"\
        "\n"\
        "-Y %u +X %u\n";

    inline size_t FindEOL(const char* str, size_t maxlen) noexcept
    {
        size_t pos = 0;

        while (pos < maxlen)
        {
            if (str[pos] == '\n')
                return pos;
            else if (str[pos] == '\0')
                return size_t(-1);
            ++pos;
        }

        return 0;
    }

#ifndef _WIN32
    template<size_t sizeOfBuffer>
    inline int sprintf_s(char(&buffer)[sizeOfBuffer], const char* format, ...)
    {
        // This is adapter code. It is not a full implementation of sprintf_s!
        va_list ap;
        va_start(ap, format);
        int result = vsprintf(buffer, format, ap);
        va_end(ap);
        return result;
    }
#endif

    //-------------------------------------------------------------------------------------
    // Decodes HDR header
    //-------------------------------------------------------------------------------------
    HRESULT DecodeHDRHeader(
        _In_reads_bytes_(size) const uint8_t* pSource,
        size_t size,
        _Out_ TexMetadata& metadata,
        size_t& offset,
        float& exposure) noexcept
    {
        if (!pSource)
            return E_INVALIDARG;

        memset(&metadata, 0, sizeof(TexMetadata));

        exposure = 1.f;

        if (size < sizeof(g_Signature))
        {
            return HRESULT_E_INVALID_DATA;
        }

        // Verify magic signature
        if (memcmp(pSource, g_Signature, sizeof(g_Signature) - 1) != 0
            && memcmp(pSource, g_AltSignature, sizeof(g_AltSignature) - 1) != 0)
        {
            return E_FAIL;
        }

        // Process first part of header
        bool formatFound = false;
        auto info = reinterpret_cast<const char*>(pSource);
        while (size > 0)
        {
            if (*info == '\n')
            {
                ++info;
                --size;
                break;
            }

            constexpr size_t formatLen = sizeof(g_Format) - 1;
            constexpr size_t exposureLen = sizeof(g_Exposure) - 1;
            if ((size > formatLen) && memcmp(info, g_Format, formatLen) == 0)
            {
                info += formatLen;
                size -= formatLen;

                // Trim whitespace
                while (*info == ' ' || *info == '\t')
                {
                    if (--size == 0)
                        return E_FAIL;
                    ++info;
                }

                static_assert(sizeof(g_sRGBE) == sizeof(g_sXYZE), "Format strings length mismatch");

                constexpr size_t encodingLen = sizeof(g_sRGBE) - 1;

                if (size < encodingLen)
                {
                    return E_FAIL;
                }

                if (memcmp(info, g_sRGBE, encodingLen) != 0 && memcmp(info, g_sXYZE, encodingLen) != 0)
                {
                    return HRESULT_E_NOT_SUPPORTED;
                }

                formatFound = true;

                const size_t len = FindEOL(info, size);
                if (len == size_t(-1)
                    || len < 1)
                {
                    return E_FAIL;
                }

                info += len + 1;
                size -= len + 1;
            }
            else if ((size > exposureLen) && memcmp(info, g_Exposure, exposureLen) == 0)
            {
                info += exposureLen;
                size -= exposureLen;

                // Trim whitespace
                while (*info == ' ' || *info == '\t')
                {
                    if (--size == 0)
                        return E_FAIL;
                    ++info;
                }

                const size_t len = FindEOL(info, size);
                if (len == size_t(-1)
                    || len < 1)
                {
                    return E_FAIL;
                }

                char buff[32] = {};
                strncpy_s(buff, info, std::min<size_t>(31, len));

                auto newExposure = static_cast<float>(atof(buff));
                if ((newExposure >= 1e-12f) && (newExposure <= 1e12f))
                {
                    // Note that we ignore strange exposure values (like EXPOSURE=0)
                    exposure *= newExposure;
                }

                info += len + 1;
                size -= len + 1;
            }
            else
            {
                const size_t len = FindEOL(info, size);
                if (len == size_t(-1)
                    || len < 1)
                {
                    return E_FAIL;
                }

                info += len + 1;
                size -= len + 1;
            }
        }

        if (!formatFound)
        {
            return E_FAIL;
        }

        // Get orientation
        char orientation[256] = {};

        const size_t len = FindEOL(info, std::min<size_t>(sizeof(orientation), size - 1));
        if (len == size_t(-1)
            || len <= 2)
        {
            return E_FAIL;
        }

        strncpy_s(orientation, info, len);

        if (orientation[0] != '-' && orientation[1] != 'Y')
        {
            // We only support the -Y +X orientation (see top of file)
            return (static_cast<unsigned long>(((orientation[0] == '+' || orientation[0] == '-') && (orientation[1] == 'X' || orientation[1] == 'Y'))))
                ? HRESULT_E_NOT_SUPPORTED : HRESULT_E_INVALID_DATA;
        }

        uint32_t height = 0;
        if (sscanf_s(orientation + 2, "%u", &height) != 1)
        {
            return E_FAIL;
        }

        if (height > UINT16_MAX)
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        const char* ptr = orientation + 2;
        while (*ptr != 0 && *ptr != '-' && *ptr != '+')
            ++ptr;

        if (*ptr == 0)
        {
            return E_FAIL;
        }
        else if (*ptr != '+')
        {
            // We only support the -Y +X orientation (see top of file)
            return HRESULT_E_NOT_SUPPORTED;
        }

        ++ptr;
        if (*ptr == 0 || (*ptr != 'X' && *ptr != 'Y'))
        {
            return E_FAIL;
        }
        else if (*ptr != 'X')
        {
            // We only support the -Y +X orientation (see top of file)
            return HRESULT_E_NOT_SUPPORTED;
        }

        ++ptr;
        uint32_t width;
        if (sscanf_s(ptr, "%u", &width) != 1)
        {
            return E_FAIL;
        }

        if (width > UINT16_MAX)
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        info += len + 1;
        size -= len + 1;

        if (!width || !height)
        {
            return HRESULT_E_INVALID_DATA;
        }

        uint64_t sizeBytes = uint64_t(width) * uint64_t(height) * sizeof(float) * 4;
        if (sizeBytes > UINT32_MAX)
        {
            return HRESULT_E_ARITHMETIC_OVERFLOW;
        }

        if (size == 0)
        {
            return E_FAIL;
        }

        offset = size_t(info - reinterpret_cast<const char*>(pSource));

        metadata.width = width;
        metadata.height = height;
        metadata.depth = metadata.arraySize = metadata.mipLevels = 1;
        metadata.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;
        metadata.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);

        return S_OK;
    }

    //-------------------------------------------------------------------------------------
    // FloatToRGBE
    //-------------------------------------------------------------------------------------
    inline void FloatToRGBE(_Out_writes_(width*4) uint8_t* pDestination, _In_reads_(width*fpp) const float* pSource, size_t width, _In_range_(3, 4) int fpp) noexcept
    {
        auto ePtr = pSource + width * size_t(fpp);

        for (size_t j = 0; j < width; ++j)
        {
            if (pSource + 2 >= ePtr) break;
            const float r = pSource[0] >= 0.f ? pSource[0] : 0.f;
            const float g = pSource[1] >= 0.f ? pSource[1] : 0.f;
            const float b = pSource[2] >= 0.f ? pSource[2] : 0.f;
            pSource += fpp;

            const float max_xy = (r > g) ? r : g;
            float max_xyz = (max_xy > b) ? max_xy : b;

            if (max_xyz > 1e-32f)
            {
                int e;
                max_xyz = frexpf(max_xyz, &e) * 256.f / max_xyz;
                e += 128;

                const uint8_t red = uint8_t(r * max_xyz);
                const uint8_t green = uint8_t(g * max_xyz);
                const uint8_t blue = uint8_t(b * max_xyz);

                pDestination[0] = red;
                pDestination[1] = green;
                pDestination[2] = blue;
                pDestination[3] = (red || green || blue) ? uint8_t(e & 0xff) : 0u;
            }
            else
            {
                pDestination[0] = pDestination[1] = pDestination[2] = pDestination[3] = 0;
            }

            pDestination += 4;
        }
    }

    //-------------------------------------------------------------------------------------
    // HalfToRGBE
    //-------------------------------------------------------------------------------------
    inline void HalfToRGBE(_Out_writes_(width * 4) uint8_t* pDestination, _In_reads_(width* fpp) const uint16_t* pSource, size_t width, _In_range_(3, 4) int fpp) noexcept
    {
        auto ePtr = pSource + width * size_t(fpp);

        for (size_t j = 0; j < width; ++j)
        {
            if (pSource + 2 >= ePtr) break;
            float r = PackedVector::XMConvertHalfToFloat(pSource[0]); r = (r >= 0.f) ? r : 0.f;
            float g = PackedVector::XMConvertHalfToFloat(pSource[1]); g = (g >= 0.f) ? g : 0.f;
            float b = PackedVector::XMConvertHalfToFloat(pSource[2]); b = (b >= 0.f) ? b : 0.f;
            pSource += fpp;

            const float max_xy = (r > g) ? r : g;
            float max_xyz = (max_xy > b) ? max_xy : b;

            if (max_xyz > 1e-32f)
            {
                int e;
                max_xyz = frexpf(max_xyz, &e) * 256.f / max_xyz;
                e += 128;

                const uint8_t red = uint8_t(r * max_xyz);
                const uint8_t green = uint8_t(g * max_xyz);
                const uint8_t blue = uint8_t(b * max_xyz);

                pDestination[0] = red;
                pDestination[1] = green;
                pDestination[2] = blue;
                pDestination[3] = (red || green || blue) ? uint8_t(e & 0xff) : 0u;
            }
            else
            {
                pDestination[0] = pDestination[1] = pDestination[2] = pDestination[3] = 0;
            }

            pDestination += 4;
        }
    }

    //-------------------------------------------------------------------------------------
    // Encode using Adapative RLE
    //-------------------------------------------------------------------------------------
    _Success_(return > 0)
        size_t EncodeRLE(_Out_writes_(width * 4) uint8_t* enc, _In_reads_(width * 4) const uint8_t* rgbe, size_t rowPitch, size_t width) noexcept
    {
        if (width < 8 || width > INT16_MAX)
        {
            // Don't try to compress too narrow or too wide scan-lines
            return 0;
        }

    #ifdef WRITE_OLD_COLORS
        size_t encSize = 0;

        const uint8_t* scanPtr = rgbe;
        for (size_t pixelCount = 0; pixelCount < width;)
        {
            size_t spanLen = 1;
            auto spanPtr = reinterpret_cast<const uint32_t*>(scanPtr);
            while (pixelCount + spanLen < width && spanLen < INT16_MAX)
            {
                if (spanPtr[spanLen] == *spanPtr)
                {
                    ++spanLen;
                }
                else
                    break;
            }

            if (spanLen > 2)
            {
                if (scanPtr[0] == 1 && scanPtr[1] == 1 && scanPtr[2] == 1)
                {
                    return 0;
                }

                if (encSize + 8 > rowPitch)
                    return 0;

                auto rleLen = static_cast<uint8_t>(std::min<size_t>(spanLen - 1, 255));

                enc[0] = scanPtr[0];
                enc[1] = scanPtr[1];
                enc[2] = scanPtr[2];
                enc[3] = scanPtr[3];
                enc[4] = 1;
                enc[5] = 1;
                enc[6] = 1;
                enc[7] = rleLen;
                enc += 8;
                encSize += 8;

                size_t remaining = spanLen - 1 - rleLen;

                if (remaining > 0)
                {
                    rleLen = static_cast<uint8_t>(remaining >> 8);

                    if (rleLen > 0)
                    {
                        if (encSize + 4 > rowPitch)
                            return 0;

                        enc[0] = 1;
                        enc[1] = 1;
                        enc[2] = 1;
                        enc[3] = rleLen;
                        enc += 4;
                        encSize += 4;

                        remaining -= (rleLen << 8);
                    }

                    while (remaining > 0)
                    {
                        if (encSize + 4 > rowPitch)
                            return 0;

                        enc[0] = scanPtr[0];
                        enc[1] = scanPtr[1];
                        enc[2] = scanPtr[2];
                        enc[3] = scanPtr[3];
                        enc += 4;
                        encSize += 4;

                        --remaining;
                    }
                }

                scanPtr += spanLen * 4;
                pixelCount += spanLen;
            }
            else if (scanPtr[0] == 1 && scanPtr[1] == 1 && scanPtr[2] == 1)
            {
                return 0;
            }
            else
            {
                if (encSize + 4 > rowPitch)
                    return 0;

                enc[0] = scanPtr[0];
                enc[1] = scanPtr[1];
                enc[2] = scanPtr[2];
                enc[3] = scanPtr[3];
                enc += 4;
                encSize += 4;
                ++pixelCount;
                scanPtr += 4;
            }
        }

        return encSize;
    #else
        enc[0] = 2;
        enc[1] = 2;
        enc[2] = uint8_t(width >> 8);
        enc[3] = uint8_t(width & 0xff);
        enc += 4;
        size_t encSize = 4;

        uint8_t scan[128] = {};

        for (int channel = 0; channel < 4; ++channel)
        {
            const uint8_t* spanPtr = rgbe + channel;
            for (size_t pixelCount = 0; pixelCount < width;)
            {
                uint8_t spanLen = 1;
                while (pixelCount + spanLen < width && spanLen < 127)
                {
                    if (spanPtr[spanLen * 4] == *spanPtr)
                    {
                        ++spanLen;
                    }
                    else
                        break;
                }

                if (spanLen > 1)
                {
                    if (encSize + 2 > rowPitch)
                        return 0;

                    enc[0] = 128u + spanLen;
                    enc[1] = *spanPtr;
                    enc += 2;
                    encSize += 2;
                    spanPtr += size_t(spanLen) * 4;
                    pixelCount += spanLen;
                }
                else
                {
                    uint8_t runLen = 1;
                    scan[0] = *spanPtr;
                    while (pixelCount + runLen < width && runLen < 127)
                    {
                        if (spanPtr[(runLen - 1) * 4] != spanPtr[runLen * 4])
                        {
                            scan[runLen] = spanPtr[runLen * 4];
                            runLen++;
                        }
                        else
                            break;
                    }

                    if (encSize + runLen + 1 > rowPitch)
                        return 0;

                    *enc++ = runLen;
                    memcpy(enc, scan, runLen);
                    enc += runLen;
                    encSize += size_t(runLen) + 1;
                    spanPtr += size_t(runLen) * 4;
                    pixelCount += runLen;
                }
            }
        }

        return encSize;
    #endif
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from HDR file in memory/on disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromHDRMemory(const uint8_t* pSource, size_t size, TexMetadata& metadata) noexcept
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    size_t offset;
    float exposure;
    return DecodeHDRHeader(pSource, size, metadata, offset, exposure);
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromHDRFile(const wchar_t* szFile, TexMetadata& metadata) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

#ifdef _WIN32
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Get the file size
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid HDR file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_E_FILE_TOO_LARGE;
    }

    const size_t len = fileInfo.EndOfFile.LowPart;
#else // !WIN32
    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (fileLen > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return E_FAIL;

    const size_t len = fileLen;
#endif

    // Need at least enough data to fill the standard header to be a valid HDR
    if (len < sizeof(g_Signature))
    {
        return E_FAIL;
    }

    // Read the first part of the file to find the header
    uint8_t header[8192] = {};

#ifdef _WIN32
    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), header, std::min<DWORD>(sizeof(header), fileInfo.EndOfFile.LowPart), &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const auto headerLen = static_cast<size_t>(bytesRead);
#else
    const auto headerLen = std::min<size_t>(sizeof(header), len);

    inFile.read(reinterpret_cast<char*>(header), headerLen);
    if (!inFile)
        return E_FAIL;
#endif

    size_t offset;
    float exposure;
    return DecodeHDRHeader(header, headerLen, metadata, offset, exposure);
}


//-------------------------------------------------------------------------------------
// Load a HDR file in memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromHDRMemory(const uint8_t* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    image.Release();

    size_t offset;
    float exposure;
    TexMetadata mdata;
    HRESULT hr = DecodeHDRHeader(pSource, size, mdata, offset, exposure);
    if (FAILED(hr))
        return hr;

    if (offset > size)
        return E_FAIL;

    const size_t remaining = size - offset;
    if (remaining == 0)
        return E_FAIL;

    hr = image.Initialize2D(mdata.format, mdata.width, mdata.height, 1, 1, CP_FLAGS_LIMIT_4GB);
    if (FAILED(hr))
        return hr;

    // Copy pixels
    auto sourcePtr = static_cast<const uint8_t*>(pSource) + offset;

    size_t pixelLen = remaining;

    const Image* img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    auto destPtr = img->pixels;

#ifdef _DEBUG
    memset(img->pixels, 0xFF, img->rowPitch * img->height);
#endif

    for (size_t scan = 0; scan < mdata.height; ++scan)
    {
        if (pixelLen < 4)
        {
            image.Release();
            return E_FAIL;
        }

        uint8_t inColor[4];
        memcpy(inColor, sourcePtr, 4);
        sourcePtr += 4;
        pixelLen -= 4;

        auto scanLine = reinterpret_cast<float*>(destPtr);

        if (inColor[0] == 2 && inColor[1] == 2 && inColor[2] < 128)
        {
            // Adaptive Run Length Encoding (RLE)
            if (size_t((size_t(inColor[2]) << 8) + inColor[3]) != mdata.width)
            {
                image.Release();
                return E_FAIL;
            }

            for (int channel = 0; channel < 4; ++channel)
            {
                auto pixelLoc = scanLine + channel;
                for (size_t pixelCount = 0; pixelCount < mdata.width;)
                {
                    if (pixelLen < 2)
                    {
                        image.Release();
                        return E_FAIL;
                    }

                    assert(sourcePtr < (reinterpret_cast<const uint8_t*>(pSource) + size));

                    uint8_t runLen = *sourcePtr;
                    if (runLen > 128)
                    {
                        runLen &= 127;
                        if (pixelCount + runLen > mdata.width)
                        {
                            image.Release();
                            return E_FAIL;
                        }

                        auto val = static_cast<float>(sourcePtr[1]);
                        for (uint8_t j = 0; j < runLen; ++j)
                        {
                            *pixelLoc = val;
                            pixelLoc += 4;
                        }
                        pixelCount += runLen;
                        sourcePtr += 2;
                        pixelLen -= 2;
                    }
                    else if ((size < size_t(runLen) + 1) || ((pixelCount + size_t(runLen)) > mdata.width))
                    {
                        image.Release();
                        return E_FAIL;
                    }
                    else
                    {
                        ++sourcePtr;
                        for (uint8_t j = 0; j < runLen; ++j)
                        {
                            auto val = static_cast<float>(*sourcePtr++);
                            *pixelLoc = val;
                            pixelLoc += 4;
                        }
                        pixelCount += runLen;
                        pixelLen -= size_t(runLen) + 1;
                    }
                }
            }
        }
        else
        {
            auto pixelLoc = scanLine;

            float prevColor[4];
            prevColor[0] = inColor[0];
            prevColor[1] = inColor[1];
            prevColor[2] = inColor[2];
            prevColor[3] = inColor[3];

            int bitShift = 0;
            for (size_t pixelCount = 0; pixelCount < mdata.width;)
            {
                if (inColor[0] == 1 && inColor[1] == 1 && inColor[2] == 1)
                {
                    if (bitShift > 24)
                    {
                        image.Release();
                        return E_FAIL;
                    }

                    // "Standard" Run Length Encoding
                    const size_t spanLen = size_t(inColor[3]) << bitShift;
                    if (spanLen + pixelCount > mdata.width)
                    {
                        image.Release();
                        return E_FAIL;
                    }

                    for (size_t j = 0; j < spanLen; ++j)
                    {
                        pixelLoc[0] = prevColor[0];
                        pixelLoc[1] = prevColor[1];
                        pixelLoc[2] = prevColor[2];
                        pixelLoc[3] = prevColor[3];
                        pixelLoc += 4;
                    }
                    pixelCount += spanLen;
                    bitShift += 8;
                }
                else
                {
                    // Uncompressed
                    pixelLoc[0] = prevColor[0] = inColor[0];
                    pixelLoc[1] = prevColor[1] = inColor[1];
                    pixelLoc[2] = prevColor[2] = inColor[2];
                    pixelLoc[3] = prevColor[3] = inColor[3];
                    bitShift = 0;
                    ++pixelCount;
                    pixelLoc += 4;
                }

                if (pixelCount >= mdata.width)
                    break;

                if (pixelLen < 4)
                {
                    image.Release();
                    return E_FAIL;
                }

                memcpy(inColor, sourcePtr, 4);
                sourcePtr += 4;
                pixelLen -= 4;
            }
        }

        destPtr += img->rowPitch;
    }

    // Transform values
    {
        auto fdata = reinterpret_cast<float*>(image.GetPixels());

        for (size_t j = 0; j < image.GetPixelsSize(); j += 16)
        {
            const auto exponent = static_cast<int>(fdata[3]);
            fdata[0] = 1.0f / exposure*ldexpf((fdata[0] + 0.5f), exponent - (128 + 8));
            fdata[1] = 1.0f / exposure*ldexpf((fdata[1] + 0.5f), exponent - (128 + 8));
            fdata[2] = 1.0f / exposure*ldexpf((fdata[2] + 0.5f), exponent - (128 + 8));
            fdata[3] = 1.f;

            fdata += 4;
        }
    }

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a HDR file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromHDRFile(const wchar_t* szFile, TexMetadata* metadata, ScratchImage& image) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

    image.Release();

#ifdef _WIN32
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Get the file size
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid HDR file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_E_FILE_TOO_LARGE;
    }

    const size_t len = fileInfo.EndOfFile.LowPart;
#else // !WIN32
    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (fileLen > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return E_FAIL;

    const size_t len = fileLen;
#endif

    // Need at least enough data to fill the header to be a valid HDR
    if (len < sizeof(g_Signature))
    {
        return E_FAIL;
    }

    // Read file
    std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[len]);
    if (!temp)
    {
        return E_OUTOFMEMORY;
    }

#ifdef _WIN32
    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), temp.get(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesRead != fileInfo.EndOfFile.LowPart)
    {
        return E_FAIL;
    }
#else
    inFile.read(reinterpret_cast<char*>(temp.get()), len);
    if (!inFile)
        return E_FAIL;
#endif

    return LoadFromHDRMemory(temp.get(), len, metadata, image);
}


//-------------------------------------------------------------------------------------
// Save a HDR file to memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToHDRMemory(const Image& image, Blob& blob) noexcept
{
    if (!image.pixels)
        return E_POINTER;

    if (image.width > INT16_MAX || image.height > INT16_MAX)
    {
        // Images larger than this can't be RLE encoded. They are technically allowed as
        // uncompresssed, but we just don't support them.
        return HRESULT_E_NOT_SUPPORTED;
    }

    int fpp;
    switch (image.format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        fpp = 4;
        break;
    case DXGI_FORMAT_R32G32B32_FLOAT:
        fpp = 3;
        break;

    default:
        return HRESULT_E_NOT_SUPPORTED;
    }

    blob.Release();

    char header[256] = {};
    sprintf_s(header, g_Header, image.height, image.width);

    auto headerLen = static_cast<DWORD>(strlen(header));

    size_t rowPitch = image.width * 4;
    const size_t slicePitch = image.height * rowPitch;

    HRESULT hr = blob.Initialize(headerLen + slicePitch);
    if (FAILED(hr))
        return hr;

    // Copy header
    auto dPtr = blob.GetBufferPointer();
    assert(dPtr != nullptr);
    memcpy(dPtr, header, headerLen);
    dPtr += headerLen;

#ifdef DISABLE_COMPRESS
    // Uncompressed write
    auto sPtr = reinterpret_cast<const uint8_t*>(image.pixels);
    for (size_t scan = 0; scan < image.height; ++scan)
    {
        FloatToRGBE(dPtr, reinterpret_cast<const float*>(sPtr), image.width, fpp);
        dPtr += rowPitch;
        sPtr += image.rowPitch;
    }
#else
    std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[rowPitch * 2]);
    if (!temp)
    {
        blob.Release();
        return E_OUTOFMEMORY;
    }

    auto rgbe = temp.get();
    auto enc = temp.get() + rowPitch;

    const uint8_t* sPtr = image.pixels;
    for (size_t scan = 0; scan < image.height; ++scan)
    {
        if (image.format == DXGI_FORMAT_R32G32B32A32_FLOAT || image.format == DXGI_FORMAT_R32G32B32_FLOAT)
        {
            FloatToRGBE(rgbe, reinterpret_cast<const float*>(sPtr), image.width, fpp);
        }
        else if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
            HalfToRGBE(rgbe, reinterpret_cast<const uint16_t*>(sPtr), image.width, fpp);
        }
        sPtr += image.rowPitch;

        size_t encSize = EncodeRLE(enc, rgbe, rowPitch, image.width);
        if (encSize > 0)
        {
            memcpy(dPtr, enc, encSize);
            dPtr += encSize;
        }
        else
        {
            memcpy(dPtr, rgbe, rowPitch);
            dPtr += rowPitch;
        }
    }
#endif

    hr = blob.Trim(size_t(dPtr - blob.GetConstBufferPointer()));
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a HDR file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToHDRFile(const Image& image, const wchar_t* szFile) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    if (image.width > INT16_MAX || image.height > INT16_MAX)
    {
        // Images larger than this can't be RLE encoded. They are technically allowed as
        // uncompresssed, but we just don't support them.
        return HRESULT_E_NOT_SUPPORTED;
    }

    int fpp;
    switch (image.format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        fpp = 4;
        break;
    case DXGI_FORMAT_R32G32B32_FLOAT:
        fpp = 3;
        break;

    default:
        return HRESULT_E_NOT_SUPPORTED;
    }

    // Create file and write header
#ifdef _WIN32
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto_delete_file delonfail(hFile.get());
#else // !WIN32
    std::ofstream outFile(std::filesystem::path(szFile), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!outFile)
        return E_FAIL;
#endif

    const uint64_t pitch = uint64_t(image.width) * 4u;
    const uint64_t slicePitch = uint64_t(image.height) * pitch;

    if (pitch > UINT32_MAX)
        return HRESULT_E_ARITHMETIC_OVERFLOW;

    size_t rowPitch = static_cast<size_t>(pitch);

    if (slicePitch < 65535)
    {
        // For small images, it is better to create an in-memory file and write it out
        Blob blob;

        HRESULT hr = SaveToHDRMemory(image, blob);
        if (FAILED(hr))
            return hr;

        // Write blob
    #ifdef _WIN32
        const auto bytesToWrite = static_cast<const DWORD>(blob.GetBufferSize());
        DWORD bytesWritten;
        if (!WriteFile(hFile.get(), blob.GetConstBufferPointer(), bytesToWrite, &bytesWritten, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesWritten != bytesToWrite)
        {
            return E_FAIL;
        }
    #else
        outFile.write(reinterpret_cast<const char*>(blob.GetConstBufferPointer()),
            static_cast<std::streamsize>(blob.GetBufferSize()));

        if (!outFile)
            return E_FAIL;
    #endif
    }
    else
    {
        // Otherwise, write the image one scanline at a time...
        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[rowPitch * 2]);
        if (!temp)
            return E_OUTOFMEMORY;

        auto rgbe = temp.get();

        // Write header
        char header[256] = {};
        sprintf_s(header, g_Header, image.height, image.width);

    #ifdef _WIN32
        const auto headerLen = static_cast<DWORD>(strlen(header));

        DWORD bytesWritten;
        if (!WriteFile(hFile.get(), header, headerLen, &bytesWritten, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesWritten != headerLen)
            return E_FAIL;
    #else
        outFile.write(reinterpret_cast<char*>(header), static_cast<std::streamsize>(strlen(header)));
        if (!outFile)
            return E_FAIL;
    #endif

    #ifdef DISABLE_COMPRESS
            // Uncompressed write
        auto sPtr = reinterpret_cast<const uint8_t*>(image.pixels);
        for (size_t scan = 0; scan < image.height; ++scan)
        {
            FloatToRGBE(rgbe, reinterpret_cast<const float*>(sPtr), image.width, fpp);
            sPtr += image.rowPitch;

        #ifdef _WIN32
            if (!WriteFile(hFile.get(), rgbe, static_cast<DWORD>(rowPitch), &bytesWritten, nullptr))
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesWritten != rowPitch)
                return E_FAIL;
        #else
            outFile.write(reinterpret_cast<char*>(rgbe), static_cast<std::streamsize>(rowPitch));
            if (!outFile)
                return E_FAIL;
        #endif

        }
    #else
        auto enc = temp.get() + rowPitch;

        const uint8_t* sPtr = image.pixels;
        for (size_t scan = 0; scan < image.height; ++scan)
        {
            if (image.format == DXGI_FORMAT_R32G32B32A32_FLOAT || image.format == DXGI_FORMAT_R32G32B32_FLOAT)
            {
                FloatToRGBE(rgbe, reinterpret_cast<const float*>(sPtr), image.width, fpp);
            }
            else if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
            {
                HalfToRGBE(rgbe, reinterpret_cast<const uint16_t*>(sPtr), image.width, fpp);
            }
            sPtr += image.rowPitch;

            const size_t encSize = EncodeRLE(enc, rgbe, rowPitch, image.width);
            if (encSize > 0)
            {
                if (encSize > UINT32_MAX)
                    return HRESULT_E_ARITHMETIC_OVERFLOW;

            #ifdef _WIN32
                if (!WriteFile(hFile.get(), enc, static_cast<DWORD>(encSize), &bytesWritten, nullptr))
                {
                    return HRESULT_FROM_WIN32(GetLastError());
                }

                if (bytesWritten != encSize)
                    return E_FAIL;
            #else
                outFile.write(reinterpret_cast<char*>(enc), static_cast<std::streamsize>(encSize));
                if (!outFile)
                    return E_FAIL;
            #endif
            }
            else
            {
            #ifdef _WIN32
                if (!WriteFile(hFile.get(), rgbe, static_cast<DWORD>(rowPitch), &bytesWritten, nullptr))
                {
                    return HRESULT_FROM_WIN32(GetLastError());
                }

                if (bytesWritten != rowPitch)
                    return E_FAIL;
            #else
                outFile.write(reinterpret_cast<char*>(rgbe), static_cast<std::streamsize>(rowPitch));
                if (!outFile)
                    return E_FAIL;
            #endif
            }
        }
    #endif
    }

#ifdef _WIN32
    delonfail.clear();
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Adapters for /Zc:wchar_t- clients

#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

namespace DirectX
{
    HRESULT __cdecl GetMetadataFromHDRFile(
        _In_z_ const __wchar_t* szFile,
        _Out_ TexMetadata& metadata) noexcept
    {
        return GetMetadataFromHDRFile(reinterpret_cast<const unsigned short*>(szFile), metadata);
    }

    HRESULT __cdecl LoadFromHDRFile(
        _In_z_ const __wchar_t* szFile,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image) noexcept
    {
        return LoadFromHDRFile(reinterpret_cast<const unsigned short*>(szFile), metadata, image);
    }

    HRESULT __cdecl SaveToHDRFile(
        _In_ const Image& image,
        _In_z_ const __wchar_t* szFile) noexcept
    {
        return SaveToHDRFile(image, reinterpret_cast<const unsigned short*>(szFile));
    }
}

#endif // !_NATIVE_WCHAR_T_DEFINED
