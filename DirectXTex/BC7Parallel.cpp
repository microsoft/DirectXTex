/*
    Based on codec from Convection Texture Tools
    Copyright (c) 2018 Eric Lasota

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject
    to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    -------------------------------------------------------------------------------------

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT License.

    http://go.microsoft.com/fwlink/?LinkId=248926
*/
#include "directxtexp.h"

#include "BC.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace
{
    enum AlphaMode
    {
        AlphaMode_Combined,
        AlphaMode_Separate,
        AlphaMode_None,
    };

    enum PBitMode
    {
        PBitMode_PerEndpoint,
        PBitMode_PerSubset,
        PBitMode_None
    };

    struct BC7ModeInfo
    {
        PBitMode m_pBitMode;
        AlphaMode m_alphaMode;
        int m_rgbBits;
        int m_alphaBits;
        int m_partitionBits;
        int m_numSubsets;
        int m_indexBits;
        int m_alphaIndexBits;
        bool m_hasIndexSelector;
    };

    BC7ModeInfo g_modes[] =
    {
        { PBitMode_PerEndpoint, AlphaMode_None, 4, 0, 4, 3, 3, 0, false },     // 0
        { PBitMode_PerSubset, AlphaMode_None, 6, 0, 6, 2, 3, 0, false },       // 1
        { PBitMode_None, AlphaMode_None, 5, 0, 6, 3, 2, 0, false },            // 2
        { PBitMode_PerEndpoint, AlphaMode_None, 7, 0, 6, 2, 2, 0, false },     // 3 (Mode reference has an error, P-bit is really per-endpoint)

        { PBitMode_None, AlphaMode_Separate, 5, 6, 0, 1, 2, 3, true },         // 4
        { PBitMode_None, AlphaMode_Separate, 7, 8, 0, 1, 2, 2, false },        // 5
        { PBitMode_PerEndpoint, AlphaMode_Combined, 7, 7, 0, 1, 4, 0, false }, // 6
        { PBitMode_PerEndpoint, AlphaMode_Combined, 5, 5, 6, 2, 2, 0, false }  // 7
    };

    static uint16_t g_partitionMap[64] =
    {
        0xCCCC, 0x8888, 0xEEEE, 0xECC8,
        0xC880, 0xFEEC, 0xFEC8, 0xEC80,
        0xC800, 0xFFEC, 0xFE80, 0xE800,
        0xFFE8, 0xFF00, 0xFFF0, 0xF000,
        0xF710, 0x008E, 0x7100, 0x08CE,
        0x008C, 0x7310, 0x3100, 0x8CCE,
        0x088C, 0x3110, 0x6666, 0x366C,
        0x17E8, 0x0FF0, 0x718E, 0x399C,
        0xaaaa, 0xf0f0, 0x5a5a, 0x33cc,
        0x3c3c, 0x55aa, 0x9696, 0xa55a,
        0x73ce, 0x13c8, 0x324c, 0x3bdc,
        0x6996, 0xc33c, 0x9966, 0x660,
        0x272, 0x4e4, 0x4e40, 0x2720,
        0xc936, 0x936c, 0x39c6, 0x639c,
        0x9336, 0x9cc6, 0x817e, 0xe718,
        0xccf0, 0xfcc, 0x7744, 0xee22,
    };

    static uint32_t g_partitionMap2[64] =
    {
        0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8,
        0xa5a50000, 0xa0a05050, 0x5555a0a0, 0x5a5a5050,
        0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090,
        0x94949494, 0xa4a4a4a4, 0xa9a59450, 0x2a0a4250,
        0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0,
        0xa8a85454, 0x6a6a4040, 0xa4a45000, 0x1a1a0500,
        0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400,
        0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200,
        0xa9a58000, 0x5090a0a8, 0xa8a09050, 0x24242424,
        0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50,
        0x500aa550, 0xaaaa4444, 0x66660000, 0xa5a0a5a0,
        0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600,
        0xaa444444, 0x54a854a8, 0x95809580, 0x96969600,
        0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
        0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000,
        0x40804080, 0xa9a8a9a8, 0xaaaaaa44, 0x2a4a5254,
    };

    static int g_fixupIndexes2[64] =
    {
        15,15,15,15,
        15,15,15,15,
        15,15,15,15,
        15,15,15,15,
        15, 2, 8, 2,
        2, 8, 8,15,
        2, 8, 2, 2,
        8, 8, 2, 2,

        15,15, 6, 8,
        2, 8,15,15,
        2, 8, 2, 2,
        2,15,15, 6,
        6, 2, 6, 8,
        15,15, 2, 2,
        15,15,15,15,
        15, 2, 2,15,
    };

    static int g_fixupIndexes3[64][2] =
    {
        { 3,15 },{ 3, 8 },{ 15, 8 },{ 15, 3 },
        { 8,15 },{ 3,15 },{ 15, 3 },{ 15, 8 },
        { 8,15 },{ 8,15 },{ 6,15 },{ 6,15 },
        { 6,15 },{ 5,15 },{ 3,15 },{ 3, 8 },
        { 3,15 },{ 3, 8 },{ 8,15 },{ 15, 3 },
        { 3,15 },{ 3, 8 },{ 6,15 },{ 10, 8 },
        { 5, 3 },{ 8,15 },{ 8, 6 },{ 6,10 },
        { 8,15 },{ 5,15 },{ 15,10 },{ 15, 8 },

        { 8,15 },{ 15, 3 },{ 3,15 },{ 5,10 },
        { 6,10 },{ 10, 8 },{ 8, 9 },{ 15,10 },
        { 15, 6 },{ 3,15 },{ 15, 8 },{ 5,15 },
        { 15, 3 },{ 15, 6 },{ 15, 6 },{ 15, 8 },
        { 3,15 },{ 15, 3 },{ 5,15 },{ 5,15 },
        { 5,15 },{ 8,15 },{ 5,15 },{ 10,15 },
        { 5,15 },{ 10,15 },{ 8,15 },{ 13,15 },
        { 15, 3 },{ 12,15 },{ 3,15 },{ 3, 8 },
    };

    struct InputBlock
    {
        int32_t m_pixels[16];
    };

#if (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
    // SSE2 version

    struct ParallelMath
    {
        static const int ParallelSize = 8;

        struct Int16
        {
            __m128i m_value;

            inline Int16 operator+(int16_t other) const
            {
                Int16 result;
                result.m_value = _mm_add_epi16(m_value, _mm_set1_epi16(other));
                return result;
            }

            inline Int16 operator+(Int16 other) const
            {
                Int16 result;
                result.m_value = _mm_add_epi16(m_value, other.m_value);
                return result;
            }

            inline Int16 operator|(Int16 other) const
            {
                Int16 result;
                result.m_value = _mm_or_si128(m_value, other.m_value);
                return result;
            }

            inline Int16 operator-(Int16 other) const
            {
                Int16 result;
                result.m_value = _mm_sub_epi16(m_value, other.m_value);
                return result;
            }

            inline Int16 operator*(const Int16& other) const
            {
                Int16 result;
                result.m_value = _mm_mullo_epi16(m_value, other.m_value);
                return result;
            }

            inline Int16 operator<<(int bits) const
            {
                Int16 result;
                result.m_value = _mm_slli_epi16(m_value, bits);
                return result;
            }
        };

        struct Int32
        {
            __m128i m_values[2];
        };

        struct Float
        {
            __m128 m_values[2];

            inline Float operator+(const Float& other) const
            {
                Float result;
                result.m_values[0] = _mm_add_ps(m_values[0], other.m_values[0]);
                result.m_values[1] = _mm_add_ps(m_values[1], other.m_values[1]);
                return result;
            }

            inline Float operator-(const Float& other) const
            {
                Float result;
                result.m_values[0] = _mm_sub_ps(m_values[0], other.m_values[0]);
                result.m_values[1] = _mm_sub_ps(m_values[1], other.m_values[1]);
                return result;
            }

            inline Float operator*(const Float& other) const
            {
                Float result;
                result.m_values[0] = _mm_mul_ps(m_values[0], other.m_values[0]);
                result.m_values[1] = _mm_mul_ps(m_values[1], other.m_values[1]);
                return result;
            }

            inline Float operator*(float other) const
            {
                Float result;
                result.m_values[0] = _mm_mul_ps(m_values[0], _mm_set1_ps(other));
                result.m_values[1] = _mm_mul_ps(m_values[1], _mm_set1_ps(other));
                return result;
            }

            inline Float operator/(const Float& other) const
            {
                Float result;
                result.m_values[0] = _mm_div_ps(m_values[0], other.m_values[0]);
                result.m_values[1] = _mm_div_ps(m_values[1], other.m_values[1]);
                return result;
            }

            inline Float operator/(float other) const
            {
                Float result;
                result.m_values[0] = _mm_div_ps(m_values[0], _mm_set1_ps(other));
                result.m_values[1] = _mm_div_ps(m_values[1], _mm_set1_ps(other));
                return result;
            }
        };

        struct Int16CompFlag
        {
            __m128i m_value;
        };

        struct FloatCompFlag
        {
            __m128 m_values[2];
        };

        static Float Select(FloatCompFlag flag, Float a, Float b)
        {
            Float result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_or_ps(_mm_and_ps(flag.m_values[i], a.m_values[i]), _mm_andnot_ps(flag.m_values[i], b.m_values[i]));
            return result;
        }

        static Int16 Select(Int16CompFlag flag, Int16 a, Int16 b)
        {
            Int16 result;
            result.m_value = _mm_or_si128(_mm_and_si128(flag.m_value, a.m_value), _mm_andnot_si128(flag.m_value, b.m_value));
            return result;
        }

        static void ConditionalSet(Int16& dest, Int16CompFlag flag, const Int16 src)
        {
            dest.m_value = _mm_or_si128(_mm_andnot_si128(flag.m_value, dest.m_value), _mm_and_si128(flag.m_value, src.m_value));
        }

        static void ConditionalSet(Float& dest, FloatCompFlag flag, const Float src)
        {
            for (int i = 0; i < 2; i++)
                dest.m_values[i] = _mm_or_ps(_mm_andnot_ps(flag.m_values[i], dest.m_values[i]), _mm_and_ps(flag.m_values[i], src.m_values[i]));
        }

        static void MakeSafeDenominator(Float& v)
        {
            ConditionalSet(v, Equal(v, MakeFloatZero()), MakeFloat(1.0f));
        }

        static Int16 Min(Int16 a, Int16 b)
        {
            Int16 result;
            result.m_value = _mm_min_epi16(a.m_value, b.m_value);
            return result;
        }

        static Float Min(Float a, Float b)
        {
            Float result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_min_ps(a.m_values[i], b.m_values[i]);
            return result;
        }

        static Int16 Max(Int16 a, Int16 b)
        {
            Int16 result;
            result.m_value = _mm_max_epi16(a.m_value, b.m_value);
            return result;
        }

        static Float Max(Float a, Float b)
        {
            Float result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_max_ps(a.m_values[i], b.m_values[i]);
            return result;
        }

        static Float Clamp(Float v, float min, float max)
        {
            Float result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_max_ps(_mm_min_ps(v.m_values[i], _mm_set1_ps(max)), _mm_set1_ps(min));
            return result;
    }

        static void ReadPackedInputs(const InputBlock* inputBlocks, int pxOffset, Int32& outPackedPx)
        {
            for (int i = 0; i < 4; i++)
                reinterpret_cast<int32_t*>(&outPackedPx.m_values[0])[i] = inputBlocks[i].m_pixels[pxOffset];
            for (int i = 0; i < 4; i++)
                reinterpret_cast<int32_t*>(&outPackedPx.m_values[1])[i] = inputBlocks[i + 4].m_pixels[pxOffset];
        }

        static void UnpackChannel(Int32 inputPx, int ch, Int16& chOut)
        {
            __m128i ch0 = _mm_srli_epi32(inputPx.m_values[0], ch * 8);
            __m128i ch1 = _mm_srli_epi32(inputPx.m_values[1], ch * 8);
            ch0 = _mm_and_si128(ch0, _mm_set1_epi32(0xff));
            ch1 = _mm_and_si128(ch1, _mm_set1_epi32(0xff));

            chOut.m_value = _mm_packs_epi32(ch0, ch1);
        }

        static Float MakeFloat(float v)
        {
            Float f;
            f.m_values[0] = f.m_values[1] = _mm_set1_ps(v);
            return f;
        }

        static Float MakeFloatZero()
        {
            Float f;
            f.m_values[0] = f.m_values[1] = _mm_setzero_ps();
            return f;
        }

        static Int16 MakeUInt16(uint16_t v)
        {
            Int16 result;
            result.m_value = _mm_set1_epi16(static_cast<short>(v));
            return result;
        }

        static uint16_t ExtractUInt16(const Int16& v, int offset)
        {
            return reinterpret_cast<const uint16_t*>(&v)[offset];
        }

        static float ExtractFloat(float v, int offset)
        {
            return reinterpret_cast<const float*>(&v)[offset];
        }

        static Int16CompFlag Less(Int16 a, Int16 b)
        {
            Int16CompFlag result;
            result.m_value = _mm_cmplt_epi16(a.m_value, b.m_value);
            return result;
        }

        static FloatCompFlag Less(Float a, Float b)
        {
            FloatCompFlag result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_cmplt_ps(a.m_values[i], b.m_values[i]);
            return result;
        }

        static Int16CompFlag Equal(Int16 a, Int16 b)
        {
            Int16CompFlag result;
            result.m_value = _mm_cmpeq_epi16(a.m_value, b.m_value);
            return result;
        }

        static FloatCompFlag Equal(Float a, Float b)
        {
            FloatCompFlag result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_cmpeq_ps(a.m_values[i], b.m_values[i]);
            return result;
        }

        static Float UInt16ToFloat(Int16 v)
        {
            Float result;
            result.m_values[0] = _mm_cvtepi32_ps(_mm_unpacklo_epi16(v.m_value, _mm_setzero_si128()));
            result.m_values[1] = _mm_cvtepi32_ps(_mm_unpackhi_epi16(v.m_value, _mm_setzero_si128()));
            return result;
        }

        static Int16CompFlag FloatFlagToInt16(FloatCompFlag v)
        {
            __m128i lo = _mm_castps_si128(v.m_values[0]);
            __m128i hi = _mm_castps_si128(v.m_values[1]);

            Int16CompFlag result;
            result.m_value = _mm_packs_epi32(lo, hi);
            return result;
        }

        static Int16 FloatToUInt16(Float v)
        {
            __m128 half = _mm_set1_ps(0.5f);
            __m128i lo = _mm_cvttps_epi32(_mm_add_ps(v.m_values[0], half));
            __m128i hi = _mm_cvttps_epi32(_mm_add_ps(v.m_values[1], half));

            Int16 result;
            result.m_value = _mm_packs_epi32(lo, hi);
            return result;
        }

        static Float Sqrt(Float f)
        {
            Float result;
            for (int i = 0; i < 2; i++)
                result.m_values[i] = _mm_sqrt_ps(f.m_values[i]);
            return result;
        }

        static Int16 SqDiff(Int16 a, Int16 b)
        {
            __m128i diff = _mm_sub_epi16(a.m_value, b.m_value);

            Int16 result;
            result.m_value = _mm_mullo_epi16(diff, diff);
            return result;
        }

        static Int16 UnsignedRightShift(Int16 v, int bits)
        {
            Int16 result;
            result.m_value = _mm_srli_epi16(v.m_value, bits);
            return result;
        }

        static bool AnySet(Int16CompFlag v)
        {
            return _mm_movemask_epi8(v.m_value) != 0;
        }
    };

#else
    // Scalar version

    struct ParallelMath
    {
        static const int ParallelSize = 1;

        typedef float Float;
        typedef int16_t Int16;
        typedef int32_t Int32;
        typedef bool Int16CompFlag;
        typedef bool FloatCompFlag;

        template<class T>
        inline static void ConditionalSet(T& dest, bool flag, const T src)
        {
            if (flag)
                dest = src;
        }

        static void MakeSafeDenominator(float& v)
        {
            if (v == 0.f)
                v = 1.0f;
        }

        template<class T>
        inline static T Select(bool flag, T a, T b)
        {
            return flag ? a : b;
        }

        template<class T>
        inline static T Min(T a, T b)
        {
            if (a < b)
                return a;
            return b;
        }

        template<class T>
        inline static T Max(T a, T b)
        {
            if (a > b)
                return a;
            return b;
        }

        template<class T>
        inline static T Clamp(T v, T min, T max)
        {
            return Max(Min(v, max), min);
        }

        inline static void ReadPackedInputs(const InputBlock* inputBlocks, int pxOffset, Int32& outPackedPx)
        {
            outPackedPx = inputBlocks[0].m_pixels[pxOffset];
        }

        inline static void UnpackChannel(Int32 inputPx, int ch, Int16& chOut)
        {
            chOut = static_cast<uint16_t>((inputPx >> (ch * 8)) & 0xff);
        }

        inline static float MakeFloat(float v)
        {
            return v;
        }

        inline static float MakeFloatZero()
        {
            return 0.f;
        }

        inline static int16_t MakeUInt16(int16_t v)
        {
            return v;
        }

        inline static int16_t ExtractUInt16(int16_t v, int offset)
        {
            (void)offset;
            return v;
        }

        inline static float ExtractFloat(float v, int offset)
        {
            (void)offset;
            return v;
        }

        template<class T>
        inline static bool Less(T a, T b)
        {
            return a < b;
        }

        template<class T>
        inline static bool Equal(T a, T b)
        {
            return a == b;
        }

        inline static float UInt16ToFloat(uint16_t v)
        {
            return static_cast<float>(v);
        }

        inline static Int16CompFlag FloatFlagToInt16(FloatCompFlag v)
        {
            return v;
        }

        inline static uint16_t FloatToUInt16(float v)
        {
            return static_cast<uint16_t>(floorf(v + 0.5f));
        }

        inline static float Sqrt(float f)
        {
            return sqrtf(f);
        }

        inline static uint16_t SqDiff(uint16_t a, uint16_t b)
        {
            int diff = static_cast<int>(a) - static_cast<int>(b);
            return static_cast<uint16_t>(diff * diff);
        }

        inline static bool AnySet(bool b)
        {
            return b;
        }

        inline static int16_t UnsignedRightShift(int16_t v, int bits)
        {
            uint32_t i = static_cast<uint32_t>(v) & 0xffff;
            return static_cast<int16_t>(i >> bits);
        }
    };

#endif

    struct PackingVector
    {
        uint32_t m_vector[4];
        int m_offset;

        void Init()
        {
            for (int i = 0; i < 4; i++)
                m_vector[i] = 0;

            m_offset = 0;
        }

        inline void Pack(uint16_t value, int bits)
        {
            int vOffset = m_offset >> 5;
            int bitOffset = m_offset & 0x1f;

            m_vector[vOffset] |= (static_cast<uint32_t>(value) << bitOffset) & static_cast<uint32_t>(0xffffffff);

            int overflowBits = bitOffset + bits - 32;
            if (overflowBits > 0)
                m_vector[vOffset + 1] |= (static_cast<uint32_t>(value) >> (bits - overflowBits));

            m_offset += bits;
        }

        inline void Flush(uint8_t* output)
        {
            assert(m_offset == 128);

            for (int v = 0; v < 4; v++)
            {
                uint32_t chunk = m_vector[v];
                for (int b = 0; b < 4; b++)
                    output[v * 4 + b] = static_cast<uint8_t>((chunk >> (b * 8)) & 0xff);
            }
        }
    };

    void ComputeTweakFactors(int tweak, int bits, float* outFactors)
    {
        int totalUnits = (1 << bits) - 1;
        int minOutsideUnits = ((tweak >> 1) & 1);
        int maxOutsideUnits = (tweak & 1);
        int insideUnits = totalUnits - minOutsideUnits - maxOutsideUnits;

        outFactors[0] = -static_cast<float>(minOutsideUnits) / static_cast<float>(insideUnits);
        outFactors[1] = static_cast<float>(maxOutsideUnits) / static_cast<float>(insideUnits) + 1.0f;
    }

    template<int TVectorSize>
    class UnfinishedEndpoints
    {
    public:
        typedef ParallelMath::Float MFloat;
        typedef ParallelMath::Int16 MInt16;

        UnfinishedEndpoints()
        {
        }

        UnfinishedEndpoints(const MFloat base[TVectorSize], const MFloat offset[TVectorSize])
        {
            for (int ch = 0; ch < TVectorSize; ch++)
                m_base[ch] = base[ch];
            for (int ch = 0; ch < TVectorSize; ch++)
                m_offset[ch] = offset[ch];
        }

        UnfinishedEndpoints(const UnfinishedEndpoints& other)
            : UnfinishedEndpoints(other.m_base, other.m_offset)
        {
        }

        void Finish(int tweak, int bits, MInt16* outEP0, MInt16* outEP1)
        {
            float tweakFactors[2];
            ComputeTweakFactors(tweak, bits, tweakFactors);

            for (int ch = 0; ch < TVectorSize; ch++)
            {
                MFloat ep0f = ParallelMath::Clamp(m_base[ch] + m_offset[ch] * tweakFactors[0], 0.0f, 255.0f);
                MFloat ep1f = ParallelMath::Clamp(m_base[ch] + m_offset[ch] * tweakFactors[1], 0.0f, 255.0f);
                outEP0[ch] = ParallelMath::FloatToUInt16(ep0f);
                outEP1[ch] = ParallelMath::FloatToUInt16(ep1f);
            }
        }

    private:
        MFloat m_base[TVectorSize];
        MFloat m_offset[TVectorSize];
    };

    template<int TMatrixSize>
    class PackedCovarianceMatrix
    {
    public:
        // 0: xx,
        // 1: xy, yy
        // 3: xz, yz, zz 
        // 6: xw, yw, zw, ww
        // ... etc.
        static const int PyramidSize = (TMatrixSize * (TMatrixSize + 1)) / 2;

        typedef ParallelMath::Float MFloat;

        PackedCovarianceMatrix()
        {
            for (int i = 0; i < PyramidSize; i++)
                m_values[i] = ParallelMath::MakeFloatZero();
        }

        void Add(const ParallelMath::Float vec[TMatrixSize], ParallelMath::Float weight)
        {
            int index = 0;
            for (int row = 0; row < TMatrixSize; row++)
            {
                for (int col = 0; col <= row; col++)
                {
                    m_values[index] = m_values[index] + vec[row] * vec[col] * weight;
                    index++;
                }
            }
        }

        void Product(MFloat outVec[TMatrixSize], const MFloat inVec[TMatrixSize])
        {
            for (int row = 0; row < TMatrixSize; row++)
            {
                MFloat sum = ParallelMath::MakeFloatZero();

                int index = (row * (row + 1)) >> 1;
                for (int col = 0; col < TMatrixSize; col++)
                {
                    sum = sum + inVec[col] * m_values[index];
                    if (col >= row)
                        index += col;
                    else
                        index++;
                }

                outVec[row] = sum;
            }
        }

    private:
        ParallelMath::Float m_values[PyramidSize];
    };

    static const int NumEndpointSelectorPasses = 3;

    template<int TVectorSize, int TIterationCount>
    class EndpointSelector
    {
    public:
        typedef ParallelMath::Float MFloat;

        EndpointSelector()
        {
            for (int ch = 0; ch < TVectorSize; ch++)
            {
                m_centroid[ch] = ParallelMath::MakeFloatZero();
                m_direction[ch] = ParallelMath::MakeFloatZero();
            }
            m_weightTotal = ParallelMath::MakeFloatZero();
            m_minDist = ParallelMath::MakeFloat(FLT_MAX);
            m_maxDist = ParallelMath::MakeFloat(-FLT_MAX);
        }

        void ContributePass(const MFloat value[TVectorSize], int pass, MFloat weight)
        {
            if (pass == 0)
                ContributeCentroid(value, weight);
            else if (pass == 1)
                ContributeDirection(value, weight);
            else if (pass == 2)
                ContributeMinMax(value);
        }

        void FinishPass(int pass)
        {
            if (pass == 0)
                FinishCentroid();
            else if (pass == 1)
                FinishDirection();
        }

        UnfinishedEndpoints<TVectorSize> GetEndpoints(const float channelWeights[TVectorSize]) const
        {
            MFloat unweightedBase[TVectorSize];
            MFloat unweightedOffset[TVectorSize];

            for (int ch = 0; ch < TVectorSize; ch++)
            {
                MFloat min = m_centroid[ch] + m_direction[ch] * m_minDist;
                MFloat max = m_centroid[ch] + m_direction[ch] * (m_maxDist - m_minDist);

                float safeWeight = channelWeights[ch];
                if (safeWeight == 0.f)
                    safeWeight = 1.0f;

                unweightedBase[ch] = min / channelWeights[ch];
                unweightedOffset[ch] = (max - min) / channelWeights[ch];
            }

            return UnfinishedEndpoints<TVectorSize>(unweightedBase, unweightedOffset);
        }

    private:
        void ContributeCentroid(const MFloat value[TVectorSize], MFloat weight)
        {
            for (int ch = 0; ch < TVectorSize; ch++)
                m_centroid[ch] = m_centroid[ch] + value[ch] * weight;
            m_weightTotal = m_weightTotal + weight;
        }

        void FinishCentroid()
        {
            MFloat denom = m_weightTotal;
            ParallelMath::MakeSafeDenominator(denom);

            for (int ch = 0; ch < TVectorSize; ch++)
                m_centroid[ch] = m_centroid[ch] / denom;
        }

        void ContributeDirection(const MFloat value[TVectorSize], MFloat weight)
        {
            MFloat diff[TVectorSize];
            for (int ch = 0; ch < TVectorSize; ch++)
                diff[ch] = value[ch] - m_centroid[ch];

            m_covarianceMatrix.Add(diff, weight);
        }

        void FinishDirection()
        {
            MFloat approx[TVectorSize];
            for (int ch = 0; ch < TVectorSize; ch++)
                approx[ch] = ParallelMath::MakeFloat(1.0f);

            for (int i = 0; i < TIterationCount; i++)
            {
                MFloat product[TVectorSize];
                m_covarianceMatrix.Product(product, approx);

                MFloat largestComponent = product[0];
                for (int ch = 1; ch < TVectorSize; ch++)
                    largestComponent = ParallelMath::Max(largestComponent, product[ch]);

                // product = largestComponent*newApprox
                ParallelMath::MakeSafeDenominator(largestComponent);
                for (int ch = 0; ch < TVectorSize; ch++)
                    approx[ch] = product[ch] / largestComponent;
            }

            // Normalize
            MFloat approxLen = ParallelMath::MakeFloatZero();
            for (int ch = 0; ch < TVectorSize; ch++)
                approxLen = approxLen + approx[ch] * approx[ch];

            approxLen = ParallelMath::Sqrt(approxLen);

            ParallelMath::MakeSafeDenominator(approxLen);

            for (int ch = 0; ch < TVectorSize; ch++)
                m_direction[ch] = approx[ch] / approxLen;
        }

        void ContributeMinMax(const MFloat value[TVectorSize])
        {
            MFloat dist = ParallelMath::MakeFloatZero();
            for (int ch = 0; ch < TVectorSize; ch++)
                dist = dist + m_direction[ch] * (value[ch] - m_centroid[ch]);

            m_minDist = ParallelMath::Min(m_minDist, dist);
            m_maxDist = ParallelMath::Max(m_maxDist, dist);
        }
        
        ParallelMath::Float m_centroid[TVectorSize];
        ParallelMath::Float m_direction[TVectorSize];
        PackedCovarianceMatrix<TVectorSize> m_covarianceMatrix;
        ParallelMath::Float m_weightTotal;

        ParallelMath::Float m_minDist;
        ParallelMath::Float m_maxDist;
    };

    template<int TVectorSize>
    class IndexSelector
    {
    public:
        typedef ParallelMath::Float MFloat;
        typedef ParallelMath::Int16 MInt16;

        void Init(const float channelWeights[TVectorSize], MInt16 endPoint[2][TVectorSize], int prec)
        {
            m_isUniform = true;
            for (int ch = 1; ch < TVectorSize; ch++)
            {
                if (channelWeights[ch] != channelWeights[0])
                    m_isUniform = false;
            }

            // To work with channel weights, we need something where:
            // pxDiff = px - ep[0]
            // epDiff = ep[1] - ep[0]
            //
            // weightedEPDiff = epDiff * channelWeights
            // normalizedWeightedAxis = weightedEPDiff / len(weightedEPDiff)
            // normalizedIndex = dot(pxDiff * channelWeights, normalizedWeightedAxis) / len(weightedEPDiff)
            // index = normalizedIndex * maxValue
            //
            // Equivalent to:
            // axis = channelWeights * maxValue * epDiff * channelWeights / lenSquared(epDiff * channelWeights)
            // index = dot(axis, pxDiff)

            for (int ep = 0; ep < 2; ep++)
                for (int ch = 0; ch < TVectorSize; ch++)
                    m_endPoint[ep][ch] = endPoint[ep][ch];

            m_prec = prec;
            m_maxValue = static_cast<float>((1 << m_prec) - 1);

            MFloat epDiffWeighted[TVectorSize];
            for (int ch = 0; ch < TVectorSize; ch++)
            {
                m_origin[ch] = ParallelMath::UInt16ToFloat(endPoint[0][ch]);

                epDiffWeighted[ch] = (ParallelMath::UInt16ToFloat(endPoint[1][ch]) - m_origin[ch]) * channelWeights[ch];
            }

            MFloat lenSquared = epDiffWeighted[0] * epDiffWeighted[0];
            for (int ch = 1; ch < TVectorSize; ch++)
                lenSquared = lenSquared + epDiffWeighted[ch] * epDiffWeighted[ch];

            ParallelMath::MakeSafeDenominator(lenSquared);

            for (int ch = 0; ch < TVectorSize; ch++)
                m_axis[ch] = epDiffWeighted[ch] * (m_maxValue * channelWeights[ch]) / lenSquared;
        }

        void Reconstruct(MInt16 index, MInt16* pixel)
        {
            MInt16 weightRcp = ParallelMath::MakeUInt16(0);
            if (m_prec == 2)
                weightRcp = ParallelMath::MakeUInt16(10923);
            else if (m_prec == 3)
                weightRcp = ParallelMath::MakeUInt16(4681);
            else if (m_prec == 4)
                weightRcp = ParallelMath::MakeUInt16(2184);

            MInt16 weight = ParallelMath::UnsignedRightShift(index * weightRcp + 256, 9);

            for (int ch = 0; ch < TVectorSize; ch++)
                pixel[ch] = ParallelMath::UnsignedRightShift(((ParallelMath::MakeUInt16(64) - weight) * m_endPoint[0][ch] + weight * m_endPoint[1][ch] + ParallelMath::MakeUInt16(32)), 6);
        }

        MInt16 SelectIndex(const MInt16* pixel)
        {
            MFloat diff[TVectorSize];
            for (int ch = 0; ch < TVectorSize; ch++)
                diff[ch] = ParallelMath::UInt16ToFloat(pixel[ch]) - m_origin[ch];

            MFloat dist = diff[0] * m_axis[0];
            for (int ch = 1; ch < TVectorSize; ch++)
                dist = dist + diff[ch] * m_axis[ch];

            return ParallelMath::FloatToUInt16(ParallelMath::Clamp(dist, 0.0f, m_maxValue));
        }

    private:
        MInt16 m_endPoint[2][TVectorSize];
        MFloat m_origin[TVectorSize];
        MFloat m_axis[TVectorSize];
        int m_prec;
        float m_maxValue;
        bool m_isUniform;
    };

    // Solve for a, b where v = a*t + b
    // This allows endpoints to be mapped to where T=0 and T=1
    // Least squares from totals:
    // a = (tv - t*v/w)/(tt - t*t/w)
    // b = (v - a*t)/w
    template<int TVectorSize>
    class EndpointRefiner
    {
    public:
        typedef ParallelMath::Float MFloat;
        typedef ParallelMath::Int16 MInt16;

        MFloat m_tv[TVectorSize];
        MFloat m_v[TVectorSize];
        MFloat m_tt;
        MFloat m_t;
        MFloat m_w;

        float m_maxIndex;
        float m_channelWeights[TVectorSize];

        void Init(int indexBits, const float channelWeights[TVectorSize])
        {
            for (int ch = 0; ch < TVectorSize; ch++)
            {
                m_tv[ch] = ParallelMath::MakeFloatZero();
                m_v[ch] = ParallelMath::MakeFloatZero();
            }
            m_tt = ParallelMath::MakeFloatZero();
            m_t = ParallelMath::MakeFloatZero();
            m_w = ParallelMath::MakeFloatZero();

            m_maxIndex = static_cast<float>((1 << indexBits) - 1);

            for (int ch = 0; ch < TVectorSize; ch++)
                m_channelWeights[ch] = channelWeights[ch];
        }

        void Contribute(const MInt16* pixel, MInt16 index, MFloat weight)
        {
            MFloat v[TVectorSize];

            for (int ch = 0; ch < TVectorSize; ch++)
                v[ch] = ParallelMath::UInt16ToFloat(pixel[ch]) * m_channelWeights[ch];

            MFloat t = ParallelMath::UInt16ToFloat(index) / m_maxIndex;

            for (int ch = 0; ch < TVectorSize; ch++)
            {
                m_tv[ch] = m_tv[ch] + weight * t * v[ch];
                m_v[ch] = m_v[ch] + weight * v[ch];
            }
            m_tt = m_tt + weight * t * t;
            m_t = m_t + weight * t;
            m_w = m_w + weight;
        }

        void GetRefinedEndpoints(MInt16 endPoint[2][TVectorSize])
        {
            // a = (tv - t*v/w)/(tt - t*t/w)
            // b = (v - a*t)/w
            MFloat w = m_w;

            ParallelMath::MakeSafeDenominator(w);

            MFloat adenom = (m_tt - m_t * m_t / w);

            ParallelMath::FloatCompFlag adenomZero = ParallelMath::Equal(adenom, ParallelMath::MakeFloatZero());
            ParallelMath::ConditionalSet(adenom, adenomZero, ParallelMath::MakeFloat(1.0f));

            for (int ch = 0; ch < TVectorSize; ch++)
            {
                /*
                if (adenom == 0.0)
                    p1 = p2 = er.v / er.w;
                else
                {
                    float4 a = (er.tv - er.t*er.v / er.w) / adenom;
                    float4 b = (er.v - a * er.t) / er.w;
                    p1 = b;
                    p2 = a + b;
                }
                */

                MFloat a = (m_tv[ch] - m_t * m_v[ch] / w) / adenom;
                MFloat b = (m_v[ch] - a * m_t) / w;

                MFloat p1 = b;
                MFloat p2 = a + b;

                ParallelMath::ConditionalSet(p1, adenomZero, (m_v[ch] / w));
                ParallelMath::ConditionalSet(p2, adenomZero, p1);

                // Unweight
                float inverseWeight = m_channelWeights[ch];
                if (inverseWeight == 0.f)
                    inverseWeight = 1.f;

                endPoint[0][ch] = ParallelMath::FloatToUInt16(ParallelMath::Clamp(p1 / inverseWeight, 0.f, 255.0f));
                endPoint[1][ch] = ParallelMath::FloatToUInt16(ParallelMath::Clamp(p2 / inverseWeight, 0.f, 255.0f));
            }
        }
    };

    class BC7Computer
    {
    public:
        static const int NumTweakRounds = 4;
        static const int NumRefineRounds = 2;

        typedef ParallelMath::Int16 MInt16;
        typedef ParallelMath::Int32 MInt32;
        typedef ParallelMath::Float MFloat;

        struct WorkInfo
        {
            MInt16 m_mode;
            MFloat m_error;
            MInt16 m_ep[3][2][4];
            MInt16 m_indexes[16];
            MInt16 m_indexes2[16];

            union
            {
                MInt16 m_partition;
                struct IndexSelectorAndRotation
                {
                    MInt16 m_indexSelector;
                    MInt16 m_rotation;
                } m_isr;
            };
        };

        static void TweakAlpha(const MInt16 original[2], int tweak, int bits, MInt16 result[2])
        {
            float tf[2];
            ComputeTweakFactors(tweak, bits, tf);

            MFloat base = ParallelMath::UInt16ToFloat(original[0]);
            MFloat offs = ParallelMath::UInt16ToFloat(original[1]) - base;

            result[0] = ParallelMath::FloatToUInt16(ParallelMath::Clamp(base + offs * tf[0], 0.0f, 255.0f));
            result[1] = ParallelMath::FloatToUInt16(ParallelMath::Clamp(base + offs * tf[1], 0.0f, 255.0f));
        }

        static void Quantize(MInt16* color, int bits, int channels)
        {
            float maxColor = static_cast<float>((1 << bits) - 1);

            for (int i = 0; i < channels; i++)
                color[i] = ParallelMath::FloatToUInt16(ParallelMath::Clamp(ParallelMath::UInt16ToFloat(color[i]) * ParallelMath::MakeFloat(1.0f / 255.0f) * maxColor, 0.f, 255.f));
        }

        static void QuantizeP(MInt16* color, int bits, uint16_t p, int channels)
        {
            uint16_t pShift = static_cast<uint16_t>(1 << (7 - bits));
            MInt16 pShiftV = ParallelMath::MakeUInt16(pShift);

            float maxColorF = static_cast<float>(255 - (1 << (7 - bits)));

            float maxQuantized = static_cast<float>((1 << bits) - 1);

            for (int ch = 0; ch < channels; ch++)
            {
                MInt16 clr = color[ch];
                if (p)
                    clr = ParallelMath::Max(clr, pShiftV) - pShiftV;

                MFloat rerangedColor = ParallelMath::UInt16ToFloat(clr) * maxQuantized / maxColorF;

                clr = ParallelMath::FloatToUInt16(ParallelMath::Clamp(rerangedColor, 0.0f, maxQuantized)) << 1;
                if (p)
                    clr = clr | ParallelMath::MakeUInt16(1);

                color[ch] = clr;
            }
        }

        static void Unquantize(MInt16* color, int bits, int channels)
        {
            for (int ch = 0; ch < channels; ch++)
            {
                MInt16 clr = color[ch];
                clr = clr << (8 - bits);
                color[ch] = clr | ParallelMath::UnsignedRightShift(clr, bits);
            }
        }

        static void CompressEndpoints0(MInt16 ep[2][4], uint16_t p[2])
        {
            for (int j = 0; j < 2; j++)
            {
                QuantizeP(ep[j], 4, p[j], 3);
                Unquantize(ep[j], 5, 3);
                ep[j][3] = ParallelMath::MakeUInt16(255);
            }
        }

        static void CompressEndpoints1(MInt16 ep[2][4], uint16_t p)
        {
            for (int j = 0; j < 2; j++)
            {
                QuantizeP(ep[j], 6, p, 3);
                Unquantize(ep[j], 7, 3);
                ep[j][3] = ParallelMath::MakeUInt16(255);
            }
        }

        static void CompressEndpoints2(MInt16 ep[2][4])
        {
            for (int j = 0; j < 2; j++)
            {
                Quantize(ep[j], 5, 3);
                Unquantize(ep[j], 5, 3);
                ep[j][3] = ParallelMath::MakeUInt16(255);
            }
        }

        static void CompressEndpoints3(MInt16 ep[2][4], uint16_t p[2])
        {
            for (int j = 0; j < 2; j++)
                QuantizeP(ep[j], 7, p[j], 3);
        }

        static void CompressEndpoints4(MInt16 epRGB[2][3], MInt16 epA[2])
        {
            for (int j = 0; j < 2; j++)
            {
                Quantize(epRGB[j], 5, 3);
                Unquantize(epRGB[j], 5, 3);

                Quantize(epA + j, 6, 1);
                Unquantize(epA + j, 6, 1);
            }
        }

        static void CompressEndpoints5(MInt16 epRGB[2][3], MInt16 epA[2])
        {
            for (int j = 0; j < 2; j++)
            {
                Quantize(epRGB[j], 7, 3);
                Unquantize(epRGB[j], 7, 3);
            }

            // Alpha is full precision
            (void)epA;
        }

        static void CompressEndpoints6(MInt16 ep[2][4], uint16_t p[2])
        {
            for (int j = 0; j < 2; j++)
                QuantizeP(ep[j], 7, p[j], 4);
        }

        static void CompressEndpoints7(MInt16 ep[2][4], uint16_t p[2])
        {
            for (int j = 0; j < 2; j++)
            {
                QuantizeP(ep[j], 5, p[j], 4);
                Unquantize(ep[j], 6, 4);
            }
        }

        template<int TVectorSize>
        static MFloat ComputeError(DWORD flags, const MInt16 reconstructed[TVectorSize], const MInt16 original[TVectorSize], const float channelWeights[TVectorSize])
        {
            MFloat error = ParallelMath::MakeFloatZero();
            if (flags & BC_FLAGS_UNIFORM)
            {
                for (int ch = 0; ch < 4; ch++)
                    error = error + ParallelMath::UInt16ToFloat(ParallelMath::SqDiff(reconstructed[ch], original[ch]));
            }
            else
            {
                for (int ch = 0; ch < 4; ch++)
                    error = error + ParallelMath::UInt16ToFloat(ParallelMath::SqDiff(reconstructed[ch], original[ch])) * ParallelMath::MakeFloat(channelWeights[ch]);
            }

            return error;
        }

        template<int TChannelCount>
        static void PreWeightPixels(MFloat preWeightedPixels[16][TChannelCount], const MInt16 pixels[16][TChannelCount], const float channelWeights[TChannelCount])
        {
            for (int px = 0; px < 16; px++)
            {
                for (int ch = 0; ch < TChannelCount; ch++)
                    preWeightedPixels[px][ch] = ParallelMath::UInt16ToFloat(pixels[px][ch]) * channelWeights[ch];
            }
        }

        static void TrySinglePlane(DWORD flags, const MInt16 pixels[16][4], const float channelWeights[4], WorkInfo& work)
        {
            MInt16 maxAlpha = ParallelMath::MakeUInt16(0);
            MInt16 minAlpha = ParallelMath::MakeUInt16(255);
            for (int px = 0; px < 16; px++)
            {
                maxAlpha = ParallelMath::Max(maxAlpha, pixels[px][3]);
                minAlpha = ParallelMath::Min(minAlpha, pixels[px][3]);
            }

            // Try RGB modes if any block has a min alpha 251 or higher
            bool allowRGBModes = ParallelMath::AnySet(ParallelMath::Less(ParallelMath::MakeUInt16(250), minAlpha));

            // Try mode 7 if any block has alpha.
            // Mode 7 is almost never selected for RGB blocks because mode 4 has very accurate 7.7.7.1 endpoints
            // and its parity bit doesn't affect alpha, meaning mode 7 can only be better in extremely specific
            // situations, and only by at most 1 unit of error per pixel.
            bool allowMode7 = ParallelMath::AnySet(ParallelMath::Less(maxAlpha, ParallelMath::MakeUInt16(255)));

            for (uint16_t mode = 0; mode <= 7; mode++)
            {
                if ((flags & BC_FLAGS_FORCE_BC7_MODE6) && mode != 6)
                    continue;

                if (!(flags & BC_FLAGS_USE_3SUBSETS) && g_modes[mode].m_numSubsets == 3)
                    continue;

                if (mode == 4 || mode == 5)
                    continue;

                if (mode < 4 && !allowRGBModes)
                    continue;

                if (mode == 7 && !allowMode7)
                    continue;

                MInt16 rgbAdjustedPixels[16][4];
                for (int px = 0; px < 16; px++)
                {
                    for (int ch = 0; ch < 3; ch++)
                        rgbAdjustedPixels[px][ch] = pixels[px][ch];

                    if (g_modes[mode].m_alphaMode == AlphaMode_None)
                        rgbAdjustedPixels[px][3] = ParallelMath::MakeUInt16(255);
                    else
                        rgbAdjustedPixels[px][3] = pixels[px][3];
                }

                unsigned int numPartitions = 1 << g_modes[mode].m_partitionBits;
                int numSubsets = g_modes[mode].m_numSubsets;
                int indexPrec = g_modes[mode].m_indexBits;

                int parityBitMax = 1;
                if (g_modes[mode].m_pBitMode == PBitMode_PerEndpoint)
                    parityBitMax = 4;
                else if (g_modes[mode].m_pBitMode == PBitMode_PerSubset)
                    parityBitMax = 2;

                for (uint16_t partition = 0; partition < numPartitions; partition++)
                {
                    EndpointSelector<4, 8> epSelectors[3];

                    for (int epPass = 0; epPass < NumEndpointSelectorPasses; epPass++)
                    {
                        MFloat preWeightedPixels[16][4];

                        PreWeightPixels<4>(preWeightedPixels, rgbAdjustedPixels, channelWeights);

                        for (int px = 0; px < 16; px++)
                        {
                            int subset = 0;
                            if (numSubsets == 2)
                                subset = (g_partitionMap[partition] >> px) & 1;
                            else if (numSubsets == 3)
                                subset = g_partitionMap2[partition] >> (px * 2) & 3;

                            assert(subset < 3);

                            epSelectors[subset].ContributePass(preWeightedPixels[px], epPass, ParallelMath::MakeFloat(1.0f));
                        }

                        for (int subset = 0; subset < numSubsets; subset++)
                            epSelectors[subset].FinishPass(epPass);
                    }

                    UnfinishedEndpoints<4> unfinishedEPs[3];
                    for (int subset = 0; subset < numSubsets; subset++)
                        unfinishedEPs[subset] = epSelectors[subset].GetEndpoints(channelWeights);

                    MInt16 bestIndexes[16];
                    MInt16 bestEP[3][2][4];
                    MFloat bestSubsetError[3] = { ParallelMath::MakeFloat(FLT_MAX), ParallelMath::MakeFloat(FLT_MAX), ParallelMath::MakeFloat(FLT_MAX) };

                    for (int px = 0; px < 16; px++)
                        bestIndexes[px] = ParallelMath::MakeUInt16(0);

                    for (int tweak = 0; tweak < NumTweakRounds; tweak++)
                    {
                        MInt16 baseEP[3][2][4];

                        for (int subset = 0; subset < numSubsets; subset++)
                            unfinishedEPs[subset].Finish(tweak, indexPrec, baseEP[subset][0], baseEP[subset][1]);

                        for (int pIter = 0; pIter < parityBitMax; pIter++)
                        {
                            uint16_t p[2];
                            p[0] = (pIter & 1);
                            p[1] = ((pIter >> 1) & 1);

                            MInt16 ep[3][2][4];

                            for (int subset = 0; subset < numSubsets; subset++)
                                for (int epi = 0; epi < 2; epi++)
                                    for (int ch = 0; ch < 4; ch++)
                                        ep[subset][epi][ch] = baseEP[subset][epi][ch];

                            for (int refine = 0; refine < NumRefineRounds; refine++)
                            {
                                switch (mode)
                                {
                                case 0:
                                    for (int subset = 0; subset < 3; subset++)
                                        CompressEndpoints0(ep[subset], p);
                                    break;
                                case 1:
                                    for (int subset = 0; subset < 2; subset++)
                                        CompressEndpoints1(ep[subset], p[0]);
                                    break;
                                case 2:
                                    for (int subset = 0; subset < 3; subset++)
                                        CompressEndpoints2(ep[subset]);
                                    break;
                                case 3:
                                    for (int subset = 0; subset < 2; subset++)
                                        CompressEndpoints3(ep[subset], p);
                                    break;
                                case 6:
                                    CompressEndpoints6(ep[0], p);
                                    break;
                                case 7:
                                    for (int subset = 0; subset < 2; subset++)
                                        CompressEndpoints7(ep[subset], p);
                                    break;
                                default:
                                    assert(false);
                                    break;
                                };

                                IndexSelector<4> indexSelectors[3];

                                for (int subset = 0; subset < numSubsets; subset++)
                                    indexSelectors[subset].Init(channelWeights, ep[subset], indexPrec);

                                EndpointRefiner<4> epRefiners[3];

                                for (int subset = 0; subset < numSubsets; subset++)
                                    epRefiners[subset].Init(indexPrec, channelWeights);

                                MFloat subsetError[3] = { ParallelMath::MakeFloatZero(), ParallelMath::MakeFloatZero(), ParallelMath::MakeFloatZero() };

                                MInt16 indexes[16];

                                for (int px = 0; px < 16; px++)
                                {
                                    int subset = 0;
                                    if (numSubsets == 2)
                                        subset = (g_partitionMap[partition] >> px) & 1;
                                    else if (numSubsets == 3)
                                        subset = g_partitionMap2[partition] >> (px * 2) & 3;

                                    assert(subset < 3);

                                    MInt16 index = indexSelectors[subset].SelectIndex(rgbAdjustedPixels[px]);

                                    epRefiners[subset].Contribute(rgbAdjustedPixels[px], index, ParallelMath::MakeFloat(1.0f));

                                    MInt16 reconstructed[4];

                                    indexSelectors[subset].Reconstruct(index, reconstructed);

                                    subsetError[subset] = subsetError[subset] + ComputeError<4>(flags, reconstructed, pixels[px], channelWeights);

                                    indexes[px] = index;
                                }

                                ParallelMath::FloatCompFlag subsetErrorBetter[3];
                                ParallelMath::Int16CompFlag subsetErrorBetter16[3];

                                bool anyImprovements = false;
                                for (int subset = 0; subset < numSubsets; subset++)
                                {
                                    subsetErrorBetter[subset] = ParallelMath::Less(subsetError[subset], bestSubsetError[subset]);
                                    subsetErrorBetter16[subset] = ParallelMath::FloatFlagToInt16(subsetErrorBetter[subset]);

                                    if (ParallelMath::AnySet(subsetErrorBetter16[subset]))
                                    {
                                        ParallelMath::ConditionalSet(bestSubsetError[subset], subsetErrorBetter[subset], subsetError[subset]);
                                        for (int epi = 0; epi < 2; epi++)
                                            for (int ch = 0; ch < 4; ch++)
                                                ParallelMath::ConditionalSet(bestEP[subset][epi][ch], subsetErrorBetter16[subset], ep[subset][epi][ch]);

                                        anyImprovements = true;
                                    }
                                }

                                if (anyImprovements)
                                {
                                    for (int px = 0; px < 16; px++)
                                    {
                                        int subset = 0;
                                        if (numSubsets == 2)
                                            subset = (g_partitionMap[partition] >> px) & 1;
                                        else if (numSubsets == 3)
                                            subset = g_partitionMap2[partition] >> (px * 2) & 3;

                                        ParallelMath::ConditionalSet(bestIndexes[px], subsetErrorBetter16[subset], indexes[px]);
                                    }
                                }

                                if (refine != NumRefineRounds - 1)
                                {
                                    for (int subset = 0; subset < numSubsets; subset++)
                                        epRefiners[subset].GetRefinedEndpoints(ep[subset]);
                                }
                            } // refine
                        } // p
                    } // tweak

                    MFloat totalError = bestSubsetError[0];
                    for (int subset = 1; subset < numSubsets; subset++)
                        totalError = totalError + bestSubsetError[subset];

                    ParallelMath::FloatCompFlag errorBetter = ParallelMath::Less(totalError, work.m_error);
                    ParallelMath::Int16CompFlag errorBetter16 = ParallelMath::FloatFlagToInt16(errorBetter);

                    if (ParallelMath::AnySet(errorBetter16))
                    {
                        work.m_error = ParallelMath::Min(totalError, work.m_error);
                        ParallelMath::ConditionalSet(work.m_mode, errorBetter16, ParallelMath::MakeUInt16(mode));
                        ParallelMath::ConditionalSet(work.m_partition, errorBetter16, ParallelMath::MakeUInt16(partition));

                        for (int px = 0; px < 16; px++)
                            ParallelMath::ConditionalSet(work.m_indexes[px], errorBetter16, bestIndexes[px]);

                        for (int subset = 0; subset < numSubsets; subset++)
                            for (int epi = 0; epi < 2; epi++)
                                for (int ch = 0; ch < 4; ch++)
                                    ParallelMath::ConditionalSet(work.m_ep[subset][epi][ch], errorBetter16, bestEP[subset][epi][ch]);
                    }
                }
            }
        }

        static void TryDualPlane(DWORD flags, const MInt16 pixels[16][4], const float channelWeights[4], WorkInfo& work)
        {
            // TODO: These error calculations are not optimal for weight-by-alpha, but this routine needs to be mostly rewritten for that.
            // The alpha/color solutions are co-dependent in that case, but a good way to solve it would probably be to
            // solve the alpha channel first, then solve the RGB channels, which in turn breaks down into two cases:
            // - Separate alpha channel, then weighted RGB
            // - Alpha+2 other channels, then the independent channel

            if (flags & BC_FLAGS_FORCE_BC7_MODE6)
                return; // Mode 6 is not a dual-plane mode, skip it

            for (uint16_t mode = 4; mode <= 5; mode++)
            {
                for (uint16_t rotation = 0; rotation < 4; rotation++)
                {
                    int alphaChannel = (rotation + 3) & 3;
                    int redChannel = (rotation == 1) ? 3 : 0;
                    int greenChannel = (rotation == 2) ? 3 : 1;
                    int blueChannel = (rotation == 3) ? 3 : 2;

                    MInt16 rotatedRGB[16][3];

                    for (int px = 0; px < 16; px++)
                    {
                        rotatedRGB[px][0] = pixels[px][redChannel];
                        rotatedRGB[px][1] = pixels[px][greenChannel];
                        rotatedRGB[px][2] = pixels[px][blueChannel];
                    }

                    uint16_t maxIndexSelector = (mode == 4) ? 2 : 1;

                    float rotatedRGBWeights[3] = { channelWeights[redChannel], channelWeights[greenChannel], channelWeights[blueChannel] };
                    float rotatedAlphaWeight[1] = { channelWeights[alphaChannel] };

                    float uniformWeight[1] = { 1.0f };   // Since the alpha channel is independent, there's no need to bother with weights when doing refinement or selection, only error

                    MFloat preWeightedRotatedRGB[16][3];
                    PreWeightPixels<3>(preWeightedRotatedRGB, rotatedRGB, rotatedRGBWeights);

                    for (uint16_t indexSelector = 0; indexSelector < maxIndexSelector; indexSelector++)
                    {
                        EndpointSelector<3, 8> rgbSelector;

                        for (int epPass = 0; epPass < NumEndpointSelectorPasses; epPass++)
                        {
                            for (int px = 0; px < 16; px++)
                                rgbSelector.ContributePass(preWeightedRotatedRGB[px], epPass, ParallelMath::MakeFloat(1.0f));

                            rgbSelector.FinishPass(epPass);
                        }

                        MInt16 alphaRange[2];

                        alphaRange[0] = alphaRange[1] = pixels[0][alphaChannel];
                        for (int px = 1; px < 16; px++)
                        {
                            alphaRange[0] = ParallelMath::Min(pixels[px][alphaChannel], alphaRange[0]);
                            alphaRange[1] = ParallelMath::Max(pixels[px][alphaChannel], alphaRange[1]);
                        }

                        int rgbPrec = 0;
                        int alphaPrec = 0;

                        if (mode == 4)
                        {
                            rgbPrec = indexSelector ? 3 : 2;
                            alphaPrec = indexSelector ? 2 : 3;
                        }
                        else
                            rgbPrec = alphaPrec = 2;

                        UnfinishedEndpoints<3> unfinishedRGB = rgbSelector.GetEndpoints(rotatedRGBWeights);

                        MFloat bestRGBError = ParallelMath::MakeFloat(FLT_MAX);
                        MFloat bestAlphaError = ParallelMath::MakeFloat(FLT_MAX);

                        MInt16 bestRGBIndexes[16];
                        MInt16 bestAlphaIndexes[16];
                        MInt16 bestEP[2][4];

                        for (int px = 0; px < 16; px++)
                            bestRGBIndexes[px] = bestAlphaIndexes[px] = ParallelMath::MakeUInt16(0);

                        for (int tweak = 0; tweak < NumTweakRounds; tweak++)
                        {
                            MInt16 rgbEP[2][3];
                            MInt16 alphaEP[2];

                            unfinishedRGB.Finish(tweak, rgbPrec, rgbEP[0], rgbEP[1]);

                            TweakAlpha(alphaRange, tweak, alphaPrec, alphaEP);

                            for (int refine = 0; refine < NumRefineRounds; refine++)
                            {
                                if (mode == 4)
                                    CompressEndpoints4(rgbEP, alphaEP);
                                else
                                    CompressEndpoints5(rgbEP, alphaEP);


                                IndexSelector<1> alphaIndexSelector;
                                IndexSelector<3> rgbIndexSelector;

                                {
                                    MInt16 alphaEPTemp[2][1] = { { alphaEP[0] },{ alphaEP[1] } };
                                    alphaIndexSelector.Init(uniformWeight, alphaEPTemp, alphaPrec);
                                }
                                rgbIndexSelector.Init(rotatedRGBWeights, rgbEP, rgbPrec);

                                EndpointRefiner<3> rgbRefiner;
                                EndpointRefiner<1> alphaRefiner;

                                rgbRefiner.Init(rgbPrec, rotatedRGBWeights);
                                alphaRefiner.Init(alphaPrec, uniformWeight);

                                MFloat errorRGB = ParallelMath::MakeFloatZero();
                                MFloat errorA = ParallelMath::MakeFloatZero();

                                MInt16 rgbIndexes[16];
                                MInt16 alphaIndexes[16];

                                for (int px = 0; px < 16; px++)
                                {
                                    MInt16 rgbIndex = rgbIndexSelector.SelectIndex(rotatedRGB[px]);
                                    MInt16 alphaIndex = alphaIndexSelector.SelectIndex(pixels[px] + alphaChannel);

                                    rgbRefiner.Contribute(rotatedRGB[px], rgbIndex, ParallelMath::MakeFloat(1.0f));
                                    alphaRefiner.Contribute(pixels[px] + alphaChannel, alphaIndex, ParallelMath::MakeFloat(1.0f));

                                    MInt16 reconstructedRGB[3];
                                    MInt16 reconstructedAlpha[1];

                                    rgbIndexSelector.Reconstruct(rgbIndex, reconstructedRGB);
                                    alphaIndexSelector.Reconstruct(alphaIndex, reconstructedAlpha);

                                    errorRGB = errorRGB + ComputeError<3>(flags, reconstructedRGB, rotatedRGB[px], rotatedRGBWeights);

                                    errorA = errorA + ComputeError<1>(flags, reconstructedAlpha, pixels[px] + alphaChannel, rotatedAlphaWeight);

                                    rgbIndexes[px] = rgbIndex;
                                    alphaIndexes[px] = alphaIndex;
                                }

                                ParallelMath::FloatCompFlag rgbBetter = ParallelMath::Less(errorRGB, bestRGBError);
                                ParallelMath::FloatCompFlag alphaBetter = ParallelMath::Less(errorA, bestAlphaError);

                                ParallelMath::Int16CompFlag rgbBetterInt16 = ParallelMath::FloatFlagToInt16(rgbBetter);
                                ParallelMath::Int16CompFlag alphaBetterInt16 = ParallelMath::FloatFlagToInt16(alphaBetter);

                                bestRGBError = ParallelMath::Min(errorRGB, bestRGBError);
                                bestAlphaError = ParallelMath::Min(errorA, bestAlphaError);

                                for (int px = 0; px < 16; px++)
                                {
                                    ParallelMath::ConditionalSet(bestRGBIndexes[px], rgbBetterInt16, rgbIndexes[px]);
                                    ParallelMath::ConditionalSet(bestAlphaIndexes[px], alphaBetterInt16, alphaIndexes[px]);
                                }

                                for (int ep = 0; ep < 2; ep++)
                                {
                                    for (int ch = 0; ch < 3; ch++)
                                        ParallelMath::ConditionalSet(bestEP[ep][ch], rgbBetterInt16, rgbEP[ep][ch]);
                                    ParallelMath::ConditionalSet(bestEP[ep][3], alphaBetterInt16, alphaEP[ep]);
                                }

                                if (refine != NumRefineRounds - 1)
                                {
                                    rgbRefiner.GetRefinedEndpoints(rgbEP);

                                    MInt16 alphaEPTemp[2][1];
                                    alphaRefiner.GetRefinedEndpoints(alphaEPTemp);

                                    for (int i = 0; i < 2; i++)
                                        alphaEP[i] = alphaEPTemp[i][0];
                                }
                            }	// refine
                        } // tweak

                        MFloat combinedError = bestRGBError + bestAlphaError;

                        ParallelMath::FloatCompFlag errorBetter = ParallelMath::Less(combinedError, work.m_error);
                        ParallelMath::Int16CompFlag errorBetter16 = ParallelMath::FloatFlagToInt16(errorBetter);

                        work.m_error = ParallelMath::Min(combinedError, work.m_error);

                        ParallelMath::ConditionalSet(work.m_mode, errorBetter16, ParallelMath::MakeUInt16(mode));
                        ParallelMath::ConditionalSet(work.m_isr.m_rotation, errorBetter16, ParallelMath::MakeUInt16(rotation));
                        ParallelMath::ConditionalSet(work.m_isr.m_indexSelector, errorBetter16, ParallelMath::MakeUInt16(indexSelector));

                        for (int px = 0; px < 16; px++)
                        {
                            ParallelMath::ConditionalSet(work.m_indexes[px], errorBetter16, indexSelector ? bestAlphaIndexes[px] : bestRGBIndexes[px]);
                            ParallelMath::ConditionalSet(work.m_indexes2[px], errorBetter16, indexSelector ? bestRGBIndexes[px] : bestAlphaIndexes[px]);
                        }

                        for (int ep = 0; ep < 2; ep++)
                            for (int ch = 0; ch < 4; ch++)
                                ParallelMath::ConditionalSet(work.m_ep[0][ep][ch], errorBetter16, bestEP[ep][ch]);
                    }
                }
            }
        }

        template<class T>
        static void Swap(T& a, T& b)
        {
            T temp = a;
            a = b;
            b = temp;
        }

        static void Pack(DWORD flags, const InputBlock* inputs, uint8_t* packedBlocks, const float channelWeights[4])
        {
            MInt16 pixels[16][4];

            for (int px = 0; px < 16; px++)
            {
                MInt32 packedPx;
                ParallelMath::ReadPackedInputs(inputs, px, packedPx);

                for (int ch = 0; ch < 4; ch++)
                    ParallelMath::UnpackChannel(packedPx, ch, pixels[px][ch]);
            }

            WorkInfo work;
            memset(&work, 0, sizeof(work));

            work.m_error = ParallelMath::MakeFloat(FLT_MAX);

            TryDualPlane(flags, pixels, channelWeights, work);
            TrySinglePlane(flags, pixels, channelWeights, work);

            for (int block = 0; block < ParallelMath::ParallelSize; block++)
            {
                PackingVector pv;
                pv.Init();

                uint16_t mode = ParallelMath::ExtractUInt16(work.m_mode, block);
                uint16_t partition = ParallelMath::ExtractUInt16(work.m_partition, block);
                uint16_t indexSelector = ParallelMath::ExtractUInt16(work.m_isr.m_indexSelector, block);

                const BC7ModeInfo& modeInfo = g_modes[mode];

                uint16_t indexes[16];
                uint16_t indexes2[16];
                uint16_t endPoints[3][2][4];

                for (int i = 0; i < 16; i++)
                {
                    indexes[i] = ParallelMath::ExtractUInt16(work.m_indexes[i], block);
                    if (modeInfo.m_alphaMode == AlphaMode_Separate)
                        indexes2[i] = ParallelMath::ExtractUInt16(work.m_indexes2[i], block);
                }

                for (int subset = 0; subset < 3; subset++)
                {
                    for (int ep = 0; ep < 2; ep++)
                    {
                        for (int ch = 0; ch < 4; ch++)
                            endPoints[subset][ep][ch] = ParallelMath::ExtractUInt16(work.m_ep[subset][ep][ch], block);
                    }
                }

                int fixups[3] = { 0, 0, 0 };

                if (modeInfo.m_alphaMode == AlphaMode_Separate)
                {
                    bool flipRGB = ((indexes[0] & (1 << (modeInfo.m_indexBits - 1))) != 0);
                    bool flipAlpha = ((indexes2[0] & (1 << (modeInfo.m_alphaIndexBits - 1))) != 0);

                    if (flipRGB)
                    {
                        uint16_t highIndex = (1 << modeInfo.m_indexBits) - 1;
                        for (int px = 0; px < 16; px++)
                            indexes[px] = highIndex - indexes[px];
                    }

                    if (flipAlpha)
                    {
                        uint16_t highIndex = (1 << modeInfo.m_alphaIndexBits) - 1;
                        for (int px = 0; px < 16; px++)
                            indexes2[px] = highIndex - indexes2[px];
                    }

                    if (indexSelector)
                        Swap(flipRGB, flipAlpha);

                    if (flipRGB)
                    {
                        for (int ch = 0; ch < 3; ch++)
                            Swap(endPoints[0][0][ch], endPoints[0][1][ch]);
                    }
                    if (flipAlpha)
                        Swap(endPoints[0][0][3], endPoints[0][1][3]);

                }
                else
                {
                    if (modeInfo.m_numSubsets == 2)
                        fixups[1] = g_fixupIndexes2[partition];
                    else if (modeInfo.m_numSubsets == 3)
                    {
                        fixups[1] = g_fixupIndexes3[partition][0];
                        fixups[2] = g_fixupIndexes3[partition][1];
                    }

                    bool flip[3] = { false, false, false };
                    for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                        flip[subset] = ((indexes[fixups[subset]] & (1 << (modeInfo.m_indexBits - 1))) != 0);

                    if (flip[0] || flip[1] || flip[2])
                    {
                        uint16_t highIndex = (1 << modeInfo.m_indexBits) - 1;
                        for (int px = 0; px < 16; px++)
                        {
                            int subset = 0;
                            if (modeInfo.m_numSubsets == 2)
                                subset = (g_partitionMap[partition] >> px) & 1;
                            else if (modeInfo.m_numSubsets == 3)
                                subset = (g_partitionMap2[partition] >> (px * 2)) & 3;

                            if (flip[subset])
                                indexes[px] = highIndex - indexes[px];
                        }

                        int maxCH = (modeInfo.m_alphaMode == AlphaMode_Combined) ? 4 : 3;
                        for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                        {
                            if (flip[subset])
                                for (int ch = 0; ch < maxCH; ch++)
                                    Swap(endPoints[subset][0][ch], endPoints[subset][1][ch]);
                        }
                    }
                }

                pv.Pack(static_cast<uint8_t>(1 << mode), mode + 1);

                if (modeInfo.m_partitionBits)
                    pv.Pack(partition, modeInfo.m_partitionBits);

                if (modeInfo.m_alphaMode == AlphaMode_Separate)
                {
                    uint16_t rotation = ParallelMath::ExtractUInt16(work.m_isr.m_rotation, block);
                    pv.Pack(rotation, 2);
                }

                if (modeInfo.m_hasIndexSelector)
                    pv.Pack(indexSelector, 1);

                // Encode RGB
                for (int ch = 0; ch < 3; ch++)
                {
                    for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                    {
                        for (int ep = 0; ep < 2; ep++)
                        {
                            uint16_t epPart = endPoints[subset][ep][ch];
                            epPart >>= (8 - modeInfo.m_rgbBits);

                            pv.Pack(epPart, modeInfo.m_rgbBits);
                        }
                    }
                }

                // Encode alpha
                if (modeInfo.m_alphaMode != AlphaMode_None)
                {
                    for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                    {
                        for (int ep = 0; ep < 2; ep++)
                        {
                            uint16_t epPart = endPoints[subset][ep][3];
                            epPart >>= (8 - modeInfo.m_alphaBits);

                            pv.Pack(epPart, modeInfo.m_alphaBits);
                        }
                    }
                }

                // Encode parity bits
                if (modeInfo.m_pBitMode == PBitMode_PerSubset)
                {
                    for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                    {
                        uint16_t epPart = endPoints[subset][0][0];
                        epPart >>= (7 - modeInfo.m_rgbBits);
                        epPart &= 1;

                        pv.Pack(epPart, 1);
                    }
                }
                else if (modeInfo.m_pBitMode == PBitMode_PerEndpoint)
                {
                    for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
                    {
                        for (int ep = 0; ep < 2; ep++)
                        {
                            uint16_t epPart = endPoints[subset][ep][0];
                            epPart >>= (7 - modeInfo.m_rgbBits);
                            epPart &= 1;

                            pv.Pack(epPart, 1);
                        }
                    }
                }

                // Encode indexes
                for (int px = 0; px < 16; px++)
                {
                    int bits = modeInfo.m_indexBits;
                    if ((px == 0) || (px == fixups[1]) || (px == fixups[2]))
                        bits--;

                    pv.Pack(indexes[px], bits);
                }

                // Encode secondary indexes
                if (modeInfo.m_alphaMode == AlphaMode_Separate)
                {
                    for (int px = 0; px < 16; px++)
                    {
                        int bits = modeInfo.m_alphaIndexBits;
                        if (px == 0)
                            bits--;

                        pv.Pack(indexes2[px], bits);
                    }
                }

                pv.Flush(packedBlocks);

                packedBlocks += 16;
            }
        }
    };
}


_Use_decl_annotations_
void DirectX::D3DXEncodeBC7Parallel(uint8_t *pBC, const XMVECTOR *pColor, DWORD flags)
{
    assert(pColor);
    assert(pBC);

    for (size_t blockBase = 0; blockBase < BC7_NUM_PARALLEL_BLOCKS; blockBase += ParallelMath::ParallelSize)
    {
        InputBlock inputBlocks[BC7_NUM_PARALLEL_BLOCKS];

        for (size_t block = 0; block < ParallelMath::ParallelSize; block++)
        {
            InputBlock& inputBlock = inputBlocks[block];

            for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i)
            {
                int32_t packedPixel = 0;
                for (size_t ch = 0; ch < 4; ch++)
                {
                    int32_t convertedValue = static_cast<int32_t>(std::max<float>(0.0f, std::min<float>(255.0f, reinterpret_cast<const float*>(pColor)[ch] * 255.0f + 0.01f)));
                    packedPixel |= (convertedValue << (ch * 8));
                }

                inputBlock.m_pixels[i] = packedPixel;
                pColor++;
            }
        }

        const float perceptualWeights[4] = { 0.2125f / 0.7154f, 1.0f, 0.0721f / 0.7154f, 1.0f };
        const float uniformWeights[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        BC7Computer::Pack(flags, inputBlocks, pBC, (flags & BC_FLAGS_UNIFORM) ? uniformWeights : perceptualWeights);

        pBC += ParallelMath::ParallelSize * 16;
    }
}
