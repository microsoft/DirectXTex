//-------------------------------------------------------------------------------------
// DirectXTexCompress.cpp
//
// DirectX Texture Library - Texture compression
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#ifdef _OPENMP
#include <omp.h>
#pragma warning(disable : 4616 6993)
#endif

#include "BC.h"

using namespace DirectX;
using namespace DirectX::Internal;

namespace
{
    constexpr uint32_t GetBCFlags(_In_ TEX_COMPRESS_FLAGS compress) noexcept
    {
        static_assert(static_cast<int>(TEX_COMPRESS_RGB_DITHER) == static_cast<int>(BC_FLAGS_DITHER_RGB), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_A_DITHER) == static_cast<int>(BC_FLAGS_DITHER_A), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_DITHER) == static_cast<int>(BC_FLAGS_DITHER_RGB | BC_FLAGS_DITHER_A), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_UNIFORM) == static_cast<int>(BC_FLAGS_UNIFORM), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_BC7_USE_3SUBSETS) == static_cast<int>(BC_FLAGS_USE_3SUBSETS), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        static_assert(static_cast<int>(TEX_COMPRESS_BC7_QUICK) == static_cast<int>(BC_FLAGS_FORCE_BC7_MODE6), "TEX_COMPRESS_* flags should match BC_FLAGS_*");
        return (compress & (BC_FLAGS_DITHER_RGB | BC_FLAGS_DITHER_A | BC_FLAGS_UNIFORM | BC_FLAGS_USE_3SUBSETS | BC_FLAGS_FORCE_BC7_MODE6));
    }

    constexpr TEX_FILTER_FLAGS GetSRGBFlags(_In_ TEX_COMPRESS_FLAGS compress) noexcept
    {
        static_assert(TEX_FILTER_SRGB_IN == 0x1000000, "TEX_FILTER_SRGB flag values don't match TEX_FILTER_SRGB_MASK");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_IN) == static_cast<int>(TEX_FILTER_SRGB_IN), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_OUT) == static_cast<int>(TEX_FILTER_SRGB_OUT), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB) == static_cast<int>(TEX_FILTER_SRGB), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        return static_cast<TEX_FILTER_FLAGS>(compress & TEX_FILTER_SRGB_MASK);
    }

    inline bool DetermineEncoderSettings(_In_ DXGI_FORMAT format, _Out_ BC_ENCODE& pfEncode, _Out_ size_t& blocksize, _Out_ TEX_FILTER_FLAGS& cflags) noexcept
    {
        switch (format)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:    pfEncode = nullptr;         blocksize = 8;   cflags = TEX_FILTER_DEFAULT; break;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:    pfEncode = D3DXEncodeBC2;   blocksize = 16;  cflags = TEX_FILTER_DEFAULT; break;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:    pfEncode = D3DXEncodeBC3;   blocksize = 16;  cflags = TEX_FILTER_DEFAULT; break;
        case DXGI_FORMAT_BC4_UNORM:         pfEncode = D3DXEncodeBC4U;  blocksize = 8;   cflags = TEX_FILTER_RGB_COPY_RED; break;
        case DXGI_FORMAT_BC4_SNORM:         pfEncode = D3DXEncodeBC4S;  blocksize = 8;   cflags = TEX_FILTER_RGB_COPY_RED; break;
        case DXGI_FORMAT_BC5_UNORM:         pfEncode = D3DXEncodeBC5U;  blocksize = 16;  cflags = TEX_FILTER_RGB_COPY_RED | TEX_FILTER_RGB_COPY_GREEN; break;
        case DXGI_FORMAT_BC5_SNORM:         pfEncode = D3DXEncodeBC5S;  blocksize = 16;  cflags = TEX_FILTER_RGB_COPY_RED | TEX_FILTER_RGB_COPY_GREEN; break;
        case DXGI_FORMAT_BC6H_UF16:         pfEncode = D3DXEncodeBC6HU; blocksize = 16;  cflags = TEX_FILTER_DEFAULT; break;
        case DXGI_FORMAT_BC6H_SF16:         pfEncode = D3DXEncodeBC6HS; blocksize = 16;  cflags = TEX_FILTER_DEFAULT; break;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:    pfEncode = D3DXEncodeBC7;   blocksize = 16;  cflags = TEX_FILTER_DEFAULT; break;
        default:                            pfEncode = nullptr;         blocksize = 0;   cflags = TEX_FILTER_DEFAULT; return false;
        }

        return true;
    }


    //-------------------------------------------------------------------------------------
    HRESULT CompressBC(
        const Image& image,
        const Image& result,
        uint32_t bcflags,
        TEX_FILTER_FLAGS srgb,
        float threshold) noexcept
    {
        if (!image.pixels || !result.pixels)
            return E_POINTER;

        assert(image.width == result.width);
        assert(image.height == result.height);

        const DXGI_FORMAT format = image.format;
        size_t sbpp = BitsPerPixel(format);
        if (!sbpp)
            return E_FAIL;

        if (sbpp < 8)
        {
            // We don't support compressing from monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_E_NOT_SUPPORTED;
        }

        // Round to bytes
        sbpp = (sbpp + 7) / 8;

        uint8_t *pDest = result.pixels;

        // Determine BC format encoder
        BC_ENCODE pfEncode;
        size_t blocksize;
        TEX_FILTER_FLAGS cflags;
        if (!DetermineEncoderSettings(result.format, pfEncode, blocksize, cflags))
            return HRESULT_E_NOT_SUPPORTED;

        XM_ALIGNED_DATA(16) XMVECTOR temp[16];
        const uint8_t *pSrc = image.pixels;
        const uint8_t *pEnd = image.pixels + image.slicePitch;
        const size_t rowPitch = image.rowPitch;
        for (size_t h = 0; h < image.height; h += 4)
        {
            const uint8_t *sptr = pSrc;
            uint8_t* dptr = pDest;
            const size_t ph = std::min<size_t>(4, image.height - h);
            size_t w = 0;
            for (size_t count = 0; (count < result.rowPitch) && (w < image.width); count += blocksize, w += 4)
            {
                const size_t pw = std::min<size_t>(4, image.width - w);
                assert(pw > 0 && ph > 0);

                const ptrdiff_t bytesLeft = pEnd - sptr;
                assert(bytesLeft > 0);
                size_t bytesToRead = std::min<size_t>(rowPitch, static_cast<size_t>(bytesLeft));
                if (!LoadScanline(&temp[0], pw, sptr, bytesToRead, format))
                    return E_FAIL;

                if (ph > 1)
                {
                    bytesToRead = std::min<size_t>(rowPitch, static_cast<size_t>(bytesLeft) - rowPitch);
                    if (!LoadScanline(&temp[4], pw, sptr + rowPitch, bytesToRead, format))
                        return E_FAIL;

                    if (ph > 2)
                    {
                        bytesToRead = std::min<size_t>(rowPitch, static_cast<size_t>(bytesLeft) - rowPitch * 2);
                        if (!LoadScanline(&temp[8], pw, sptr + rowPitch * 2, bytesToRead, format))
                            return E_FAIL;

                        if (ph > 3)
                        {
                            bytesToRead = std::min<size_t>(rowPitch, static_cast<size_t>(bytesLeft) - rowPitch * 3);
                            if (!LoadScanline(&temp[12], pw, sptr + rowPitch * 3, bytesToRead, format))
                                return E_FAIL;
                        }
                    }
                }

                if (pw != 4 || ph != 4)
                {
                    // Replicate pixels for partial block
                    static const size_t uSrc[] = { 0, 0, 0, 1 };

                    if (pw < 4)
                    {
                        for (size_t t = 0; t < ph && t < 4; ++t)
                        {
                            for (size_t s = pw; s < 4; ++s)
                            {
                            #pragma prefast(suppress: 26000, "PREFAST false positive")
                                temp[(t << 2) | s] = temp[(t << 2) | uSrc[s]];
                            }
                        }
                    }

                    if (ph < 4)
                    {
                        for (size_t t = ph; t < 4; ++t)
                        {
                            for (size_t s = 0; s < 4; ++s)
                            {
                            #pragma prefast(suppress: 26000, "PREFAST false positive")
                                temp[(t << 2) | s] = temp[(uSrc[t] << 2) | s];
                            }
                        }
                    }
                }

                ConvertScanline(temp, 16, result.format, format, cflags | srgb);

                if (pfEncode)
                    pfEncode(dptr, temp, bcflags);
                else
                    D3DXEncodeBC1(dptr, temp, threshold, bcflags);

                sptr += sbpp * 4;
                dptr += blocksize;
            }

            pSrc += rowPitch * 4;
            pDest += result.rowPitch;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
#ifdef _OPENMP
    HRESULT CompressBC_Parallel(
        const Image& image,
        const Image& result,
        uint32_t bcflags,
        TEX_FILTER_FLAGS srgb,
        float threshold) noexcept
    {
        if (!image.pixels || !result.pixels)
            return E_POINTER;

        assert(image.width == result.width);
        assert(image.height == result.height);

        const DXGI_FORMAT format = image.format;
        size_t sbpp = BitsPerPixel(format);
        if (!sbpp)
            return E_FAIL;

        if (sbpp < 8)
        {
            // We don't support compressing from monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_E_NOT_SUPPORTED;
        }

        // Round to bytes
        sbpp = (sbpp + 7) / 8;

        const uint8_t *pEnd = image.pixels + image.slicePitch;

        // Determine BC format encoder
        BC_ENCODE pfEncode;
        size_t blocksize;
        TEX_FILTER_FLAGS cflags;
        if (!DetermineEncoderSettings(result.format, pfEncode, blocksize, cflags))
            return HRESULT_E_NOT_SUPPORTED;

        // Refactored version of loop to support parallel independance
        const size_t nBlocks = std::max<size_t>(1, (image.width + 3) / 4) * std::max<size_t>(1, (image.height + 3) / 4);

        bool fail = false;

    #pragma omp parallel for
        for (int nb = 0; nb < static_cast<int>(nBlocks); ++nb)
        {
            const int nbWidth = std::max<int>(1, int((image.width + 3) / 4));

            int y = nb / nbWidth;
            const int x = (nb - (y*nbWidth)) * 4;
            y *= 4;

            assert((x >= 0) && (x < int(image.width)));
            assert((y >= 0) && (y < int(image.height)));

            const size_t rowPitch = image.rowPitch;
            const uint8_t *pSrc = image.pixels + (size_t(y)*rowPitch) + (size_t(x)*sbpp);

            uint8_t *pDest = result.pixels + (size_t(nb)*blocksize);

            const size_t ph = std::min<size_t>(4, image.height - size_t(y));
            const size_t pw = std::min<size_t>(4, image.width - size_t(x));
            assert(pw > 0 && ph > 0);

            const ptrdiff_t bytesLeft = pEnd - pSrc;
            assert(bytesLeft > 0);
            size_t bytesToRead = std::min<size_t>(rowPitch, size_t(bytesLeft));

            XM_ALIGNED_DATA(16) XMVECTOR temp[16];
            if (!LoadScanline(&temp[0], pw, pSrc, bytesToRead, format))
                fail = true;

            if (ph > 1)
            {
                bytesToRead = std::min<size_t>(rowPitch, size_t(bytesLeft) - rowPitch);
                if (!LoadScanline(&temp[4], pw, pSrc + rowPitch, bytesToRead, format))
                    fail = true;

                if (ph > 2)
                {
                    bytesToRead = std::min<size_t>(rowPitch, size_t(bytesLeft) - rowPitch * 2);
                    if (!LoadScanline(&temp[8], pw, pSrc + rowPitch * 2, bytesToRead, format))
                        fail = true;

                    if (ph > 3)
                    {
                        bytesToRead = std::min<size_t>(rowPitch, size_t(bytesLeft) - rowPitch * 3);
                        if (!LoadScanline(&temp[12], pw, pSrc + rowPitch * 3, bytesToRead, format))
                            fail = true;
                    }
                }
            }

            if (pw != 4 || ph != 4)
            {
                // Replicate pixels for partial block
                static const size_t uSrc[] = { 0, 0, 0, 1 };

                if (pw < 4)
                {
                    for (size_t t = 0; t < ph && t < 4; ++t)
                    {
                        for (size_t s = pw; s < 4; ++s)
                        {
                            temp[(t << 2) | s] = temp[(t << 2) | uSrc[s]];
                        }
                    }
                }

                if (ph < 4)
                {
                    for (size_t t = ph; t < 4; ++t)
                    {
                        for (size_t s = 0; s < 4; ++s)
                        {
                            temp[(t << 2) | s] = temp[(uSrc[t] << 2) | s];
                        }
                    }
                }
            }

            ConvertScanline(temp, 16, result.format, format, cflags | srgb);

            if (pfEncode)
                pfEncode(pDest, temp, bcflags);
            else
                D3DXEncodeBC1(pDest, temp, threshold, bcflags);
        }

        return (fail) ? E_FAIL : S_OK;
    }
#endif // _OPENMP


    //-------------------------------------------------------------------------------------
    DXGI_FORMAT DefaultDecompress(_In_ DXGI_FORMAT format) noexcept
    {
        switch (format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_R8_UNORM;

        case DXGI_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_R8_SNORM;

        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;

        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
            // We could use DXGI_FORMAT_R32G32B32_FLOAT here since BC6H is always Alpha 1.0,
            // but this format is more supported by viewers
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }


    //-------------------------------------------------------------------------------------
    HRESULT DecompressBC(_In_ const Image& cImage, _In_ const Image& result) noexcept
    {
        if (!cImage.pixels || !result.pixels)
            return E_POINTER;

        assert(cImage.width == result.width);
        assert(cImage.height == result.height);

        const DXGI_FORMAT format = result.format;
        size_t dbpp = BitsPerPixel(format);
        if (!dbpp)
            return E_FAIL;

        if (dbpp < 8)
        {
            // We don't support decompressing to monochrome (DXGI_FORMAT_R1_UNORM)
            return HRESULT_E_NOT_SUPPORTED;
        }

        // Round to bytes
        dbpp = (dbpp + 7) / 8;

        uint8_t *pDest = result.pixels;
        if (!pDest)
            return E_POINTER;

        // Promote "typeless" BC formats
        DXGI_FORMAT cformat;
        switch (cImage.format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:  cformat = DXGI_FORMAT_BC1_UNORM; break;
        case DXGI_FORMAT_BC2_TYPELESS:  cformat = DXGI_FORMAT_BC2_UNORM; break;
        case DXGI_FORMAT_BC3_TYPELESS:  cformat = DXGI_FORMAT_BC3_UNORM; break;
        case DXGI_FORMAT_BC4_TYPELESS:  cformat = DXGI_FORMAT_BC4_UNORM; break;
        case DXGI_FORMAT_BC5_TYPELESS:  cformat = DXGI_FORMAT_BC5_UNORM; break;
        case DXGI_FORMAT_BC6H_TYPELESS: cformat = DXGI_FORMAT_BC6H_UF16; break;
        case DXGI_FORMAT_BC7_TYPELESS:  cformat = DXGI_FORMAT_BC7_UNORM; break;
        default:                        cformat = cImage.format;         break;
        }

        // Determine BC format decoder
        BC_DECODE pfDecode;
        size_t sbpp;
        switch (cformat)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:    pfDecode = D3DXDecodeBC1;   sbpp = 8;   break;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:    pfDecode = D3DXDecodeBC2;   sbpp = 16;  break;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:    pfDecode = D3DXDecodeBC3;   sbpp = 16;  break;
        case DXGI_FORMAT_BC4_UNORM:         pfDecode = D3DXDecodeBC4U;  sbpp = 8;   break;
        case DXGI_FORMAT_BC4_SNORM:         pfDecode = D3DXDecodeBC4S;  sbpp = 8;   break;
        case DXGI_FORMAT_BC5_UNORM:         pfDecode = D3DXDecodeBC5U;  sbpp = 16;  break;
        case DXGI_FORMAT_BC5_SNORM:         pfDecode = D3DXDecodeBC5S;  sbpp = 16;  break;
        case DXGI_FORMAT_BC6H_UF16:         pfDecode = D3DXDecodeBC6HU; sbpp = 16;  break;
        case DXGI_FORMAT_BC6H_SF16:         pfDecode = D3DXDecodeBC6HS; sbpp = 16;  break;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:    pfDecode = D3DXDecodeBC7;   sbpp = 16;  break;
        default:
            return HRESULT_E_NOT_SUPPORTED;
        }

        XM_ALIGNED_DATA(16) XMVECTOR temp[16];
        const uint8_t *pSrc = cImage.pixels;
        const size_t rowPitch = result.rowPitch;
        for (size_t h = 0; h < cImage.height; h += 4)
        {
            const uint8_t *sptr = pSrc;
            uint8_t* dptr = pDest;
            const size_t ph = std::min<size_t>(4, cImage.height - h);
            size_t w = 0;
            for (size_t count = 0; (count < cImage.rowPitch) && (w < cImage.width); count += sbpp, w += 4)
            {
                pfDecode(temp, sptr);
                ConvertScanline(temp, 16, format, cformat, TEX_FILTER_DEFAULT);

                const size_t pw = std::min<size_t>(4, cImage.width - w);
                assert(pw > 0 && ph > 0);

                if (!StoreScanline(dptr, rowPitch, format, &temp[0], pw))
                    return E_FAIL;

                if (ph > 1)
                {
                    if (!StoreScanline(dptr + rowPitch, rowPitch, format, &temp[4], pw))
                        return E_FAIL;

                    if (ph > 2)
                    {
                        if (!StoreScanline(dptr + rowPitch * 2, rowPitch, format, &temp[8], pw))
                            return E_FAIL;

                        if (ph > 3)
                        {
                            if (!StoreScanline(dptr + rowPitch * 3, rowPitch, format, &temp[12], pw))
                                return E_FAIL;
                        }
                    }
                }

                sptr += sbpp;
                dptr += dbpp * 4;
            }

            pSrc += cImage.rowPitch;
            pDest += rowPitch * 4;
        }

        return S_OK;
    }
}

//-------------------------------------------------------------------------------------
bool DirectX::Internal::IsAlphaAllOpaqueBC(_In_ const Image& cImage) noexcept
{
    if (!cImage.pixels)
        return false;

    // Promote "typeless" BC formats
    DXGI_FORMAT cformat;
    switch (cImage.format)
    {
    case DXGI_FORMAT_BC1_TYPELESS:  cformat = DXGI_FORMAT_BC1_UNORM; break;
    case DXGI_FORMAT_BC2_TYPELESS:  cformat = DXGI_FORMAT_BC2_UNORM; break;
    case DXGI_FORMAT_BC3_TYPELESS:  cformat = DXGI_FORMAT_BC3_UNORM; break;
    case DXGI_FORMAT_BC7_TYPELESS:  cformat = DXGI_FORMAT_BC7_UNORM; break;
    default:                        cformat = cImage.format;         break;
    }

    // Determine BC format decoder
    BC_DECODE pfDecode;
    size_t sbpp;
    switch (cformat)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:    pfDecode = D3DXDecodeBC1;   sbpp = 8;   break;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:    pfDecode = D3DXDecodeBC2;   sbpp = 16;  break;
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:    pfDecode = D3DXDecodeBC3;   sbpp = 16;  break;
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:    pfDecode = D3DXDecodeBC7;   sbpp = 16;  break;
    default:
        // BC4, BC5, and BC6 don't have alpha channels
        return false;
    }

    // Scan blocks for non-opaque alpha
    static const XMVECTORF32 threshold = { { { 0.99f, 0.99f, 0.99f, 0.99f } } };

    XM_ALIGNED_DATA(16) XMVECTOR temp[16];
    const uint8_t* pPixels = cImage.pixels;
    for (size_t h = 0; h < cImage.height; h += 4)
    {
        const uint8_t* ptr = pPixels;
        const size_t ph = std::min<size_t>(4, cImage.height - h);
        size_t w = 0;
        for (size_t count = 0; (count < cImage.rowPitch) && (w < cImage.width); count += sbpp, w += 4)
        {
            pfDecode(temp, ptr);

            const size_t pw = std::min<size_t>(4, cImage.width - w);
            assert(pw > 0 && ph > 0);

            if (pw == 4 && ph == 4)
            {
                // Full blocks
                for (size_t j = 0; j < 16; ++j)
                {
                    const XMVECTOR alpha = XMVectorSplatW(temp[j]);
                    if (XMVector4Less(alpha, threshold))
                        return false;
                }
            }
            else
            {
                // Handle partial blocks
                for (size_t y = 0; y < ph; ++y)
                {
                    for (size_t x = 0; x < pw; ++x)
                    {
                        const XMVECTOR alpha = XMVectorSplatW(temp[y * 4 + x]);
                        if (XMVector4Less(alpha, threshold))
                            return false;
                    }
                }
            }

            ptr += sbpp;
        }

        pPixels += cImage.rowPitch;
    }

    return true;
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Compression
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Compress(
    const Image& srcImage,
    DXGI_FORMAT format,
    TEX_COMPRESS_FLAGS compress,
    float threshold,
    ScratchImage& image) noexcept
{
    if (IsCompressed(srcImage.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(srcImage.format) || IsPlanar(srcImage.format) || IsPalettized(srcImage.format))
        return HRESULT_E_NOT_SUPPORTED;

    // Create compressed image
    HRESULT hr = image.Initialize2D(format, srcImage.width, srcImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    // Compress single image
    if (compress & TEX_COMPRESS_PARALLEL)
    {
    #ifndef _OPENMP
        return E_NOTIMPL;
    #else
        hr = CompressBC_Parallel(srcImage, *img, GetBCFlags(compress), GetSRGBFlags(compress), threshold);
    #endif // _OPENMP
    }
    else
    {
        hr = CompressBC(srcImage, *img, GetBCFlags(compress), GetSRGBFlags(compress), threshold);
    }

    if (FAILED(hr))
        image.Release();

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::Compress(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    TEX_COMPRESS_FLAGS compress,
    float threshold,
    ScratchImage& cImages) noexcept
{
    if (!srcImages || !nimages)
        return E_INVALIDARG;

    if (IsCompressed(metadata.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    cImages.Release();

    TexMetadata mdata2 = metadata;
    mdata2.format = format;
    HRESULT hr = cImages.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != cImages.GetImageCount())
    {
        cImages.Release();
        return E_FAIL;
    }

    const Image* dest = cImages.GetImages();
    if (!dest)
    {
        cImages.Release();
        return E_POINTER;
    }

    for (size_t index = 0; index < nimages; ++index)
    {
        assert(dest[index].format == format);

        const Image& src = srcImages[index];

        if (src.width != dest[index].width || src.height != dest[index].height)
        {
            cImages.Release();
            return E_FAIL;
        }

        if ((compress & TEX_COMPRESS_PARALLEL))
        {
        #ifndef _OPENMP
            return E_NOTIMPL;
        #else
            if (compress & TEX_COMPRESS_PARALLEL)
            {
                hr = CompressBC_Parallel(src, dest[index], GetBCFlags(compress), GetSRGBFlags(compress), threshold);
                if (FAILED(hr))
                {
                    cImages.Release();
                    return  hr;
                }
            }
        #endif // _OPENMP
        }
        else
        {
            hr = CompressBC(src, dest[index], GetBCFlags(compress), GetSRGBFlags(compress), threshold);
            if (FAILED(hr))
            {
                cImages.Release();
                return hr;
            }
        }
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Decompression
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Decompress(
    const Image& cImage,
    DXGI_FORMAT format,
    ScratchImage& image) noexcept
{
    if (!IsCompressed(cImage.format) || IsCompressed(format))
        return E_INVALIDARG;

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        // Pick a default decompressed format based on BC input format
        format = DefaultDecompress(cImage.format);
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            // Input is not a compressed format
            return E_INVALIDARG;
        }
    }
    else
    {
        if (!IsValid(format))
            return E_INVALIDARG;

        if (IsTypeless(format) || IsPlanar(format) || IsPalettized(format))
            return HRESULT_E_NOT_SUPPORTED;
    }

    // Create decompressed image
    HRESULT hr = image.Initialize2D(format, cImage.width, cImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    // Decompress single image
    hr = DecompressBC(cImage, *img);
    if (FAILED(hr))
        image.Release();

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::Decompress(
    const Image* cImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    ScratchImage& images) noexcept
{
    if (!cImages || !nimages)
        return E_INVALIDARG;

    if (!IsCompressed(metadata.format) || IsCompressed(format))
        return E_INVALIDARG;

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        // Pick a default decompressed format based on BC input format
        format = DefaultDecompress(cImages[0].format);
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            // Input is not a compressed format
            return E_FAIL;
        }
    }
    else
    {
        if (!IsValid(format))
            return E_INVALIDARG;

        if (IsTypeless(format) || IsPlanar(format) || IsPalettized(format))
            return HRESULT_E_NOT_SUPPORTED;
    }

    images.Release();

    TexMetadata mdata2 = metadata;
    mdata2.format = format;
    HRESULT hr = images.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != images.GetImageCount())
    {
        images.Release();
        return E_FAIL;
    }

    const Image* dest = images.GetImages();
    if (!dest)
    {
        images.Release();
        return E_POINTER;
    }

    for (size_t index = 0; index < nimages; ++index)
    {
        assert(dest[index].format == format);

        const Image& src = cImages[index];
        if (!IsCompressed(src.format))
        {
            images.Release();
            return E_FAIL;
        }

        if (src.width != dest[index].width || src.height != dest[index].height)
        {
            images.Release();
            return E_FAIL;
        }

        hr = DecompressBC(src, dest[index]);
        if (FAILED(hr))
        {
            images.Release();
            return hr;
        }
    }

    return S_OK;
}
