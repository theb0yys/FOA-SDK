/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CanonicalFingerprint.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr AZ::u32 RoundConstants[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
        };

        AZ::u32 RotateRight(AZ::u32 value, AZ::u32 count)
        {
            return (value >> count) | (value << (32u - count));
        }

        void TransformBlock(const AZ::u8* block, AZ::u32 state[8])
        {
            AZ::u32 words[64] = {};
            for (size_t index = 0; index < 16; ++index)
            {
                const size_t offset = index * 4;
                words[index] =
                    (static_cast<AZ::u32>(block[offset]) << 24u)
                    | (static_cast<AZ::u32>(block[offset + 1]) << 16u)
                    | (static_cast<AZ::u32>(block[offset + 2]) << 8u)
                    | static_cast<AZ::u32>(block[offset + 3]);
            }
            for (size_t index = 16; index < 64; ++index)
            {
                const AZ::u32 smallSigma0 =
                    RotateRight(words[index - 15], 7u)
                    ^ RotateRight(words[index - 15], 18u)
                    ^ (words[index - 15] >> 3u);
                const AZ::u32 smallSigma1 =
                    RotateRight(words[index - 2], 17u)
                    ^ RotateRight(words[index - 2], 19u)
                    ^ (words[index - 2] >> 10u);
                words[index] = words[index - 16]
                    + smallSigma0
                    + words[index - 7]
                    + smallSigma1;
            }

            AZ::u32 a = state[0];
            AZ::u32 b = state[1];
            AZ::u32 c = state[2];
            AZ::u32 d = state[3];
            AZ::u32 e = state[4];
            AZ::u32 f = state[5];
            AZ::u32 g = state[6];
            AZ::u32 h = state[7];

            for (size_t index = 0; index < 64; ++index)
            {
                const AZ::u32 bigSigma1 =
                    RotateRight(e, 6u) ^ RotateRight(e, 11u) ^ RotateRight(e, 25u);
                const AZ::u32 choose = (e & f) ^ ((~e) & g);
                const AZ::u32 temporary1 =
                    h + bigSigma1 + choose + RoundConstants[index] + words[index];
                const AZ::u32 bigSigma0 =
                    RotateRight(a, 2u) ^ RotateRight(a, 13u) ^ RotateRight(a, 22u);
                const AZ::u32 majority = (a & b) ^ (a & c) ^ (b & c);
                const AZ::u32 temporary2 = bigSigma0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temporary1;
                d = c;
                c = b;
                b = a;
                a = temporary1 + temporary2;
            }

            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }
    } // namespace

    AZStd::string CalculateCanonicalSha256(const AZStd::string& value)
    {
        const AZ::u64 bitLength = static_cast<AZ::u64>(value.size()) * 8u;
        size_t paddedPayloadLength = value.size() + 1;
        while ((paddedPayloadLength % 64) != 56)
        {
            ++paddedPayloadLength;
        }

        AZStd::vector<AZ::u8> bytes(paddedPayloadLength + 8, 0);
        for (size_t index = 0; index < value.size(); ++index)
        {
            bytes[index] = static_cast<AZ::u8>(
                static_cast<unsigned char>(value[index]));
        }
        bytes[value.size()] = 0x80u;
        for (size_t index = 0; index < 8; ++index)
        {
            bytes[paddedPayloadLength + index] = static_cast<AZ::u8>(
                bitLength >> (56u - static_cast<AZ::u32>(index * 8)));
        }

        AZ::u32 state[8] = {
            0x6a09e667u,
            0xbb67ae85u,
            0x3c6ef372u,
            0xa54ff53au,
            0x510e527fu,
            0x9b05688cu,
            0x1f83d9abu,
            0x5be0cd19u,
        };
        for (size_t offset = 0; offset < bytes.size(); offset += 64)
        {
            TransformBlock(bytes.data() + offset, state);
        }

        constexpr char HexDigits[] = "0123456789abcdef";
        AZStd::string result = "sha256:";
        result.reserve(71);
        for (AZ::u32 word : state)
        {
            for (int shift = 28; shift >= 0; shift -= 4)
            {
                result.push_back(HexDigits[(word >> shift) & 0x0fu]);
            }
        }
        return result;
    }

    bool CanonicalSha256Matches(
        const AZStd::string& value,
        const AZStd::string& fingerprint)
    {
        return fingerprint == CalculateCanonicalSha256(value);
    }
} // namespace TaintedGrailModdingSDK
