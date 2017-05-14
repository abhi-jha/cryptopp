// threefish.cpp - written and placed in the public domain by Jeffrey Walton
//                 Based on public domain code by Keru Kuro. Kuro's code is
//                 available at http://cppcrypto.sourceforge.net/.

#include "pch.h"
#include "config.h"

#include "threefish.h"
#include "misc.h"
#include "cpu.h"
#include "algparam.h"
#include "argnames.h"

ANONYMOUS_NAMESPACE_BEGIN

#if defined(__clang__)
# define rotatel64(x,y) rotlVariable(x,y)
# define rotater64(x,y) rotrVariable(x,y)
#else
# define rotatel64(x,y) rotlFixed(x,y)
# define rotater64(x,y) rotrFixed(x,y)
#endif

#define G256(G0, G1, G2, G3, C0, C1) \
	G0 += G1; \
	G1 = rotatel64(G1, C0) ^ G0; \
	G2 += G3; \
	G3 = rotatel64(G3, C1) ^ G2;

#define IG256(G0, G1, G2, G3, C0, C1) \
	G3 = rotater64(G3 ^ G2, C1); \
	G2 -= G3; \
	G1 = rotater64(G1 ^ G0, C0); \
	G0 -= G1; \

#define KS256(r) \
	G0 += m_rkey[(r + 1) % 5]; \
	G1 += m_rkey[(r + 2) % 5] + m_tweak[(r + 1) % 3]; \
	G2 += m_rkey[(r + 3) % 5] + m_tweak[(r + 2) % 3]; \
	G3 += m_rkey[(r + 4) % 5] + r + 1;

#define IKS256(r) \
	G0 -= m_rkey[(r + 1) % 5]; \
	G1 -= (m_rkey[(r + 2) % 5] + m_tweak[(r + 1) % 3]); \
	G2 -= (m_rkey[(r + 3) % 5] + m_tweak[(r + 2) % 3]); \
	G3 -= (m_rkey[(r + 4) % 5] + r + 1);

#define G8256(r) \
	G256(G0, G1, G2, G3, 14, 16); \
	G256(G0, G3, G2, G1, 52, 57); \
	G256(G0, G1, G2, G3, 23, 40); \
	G256(G0, G3, G2, G1, 5, 37); \
	KS256(r); \
	G256(G0, G1, G2, G3, 25, 33); \
	G256(G0, G3, G2, G1, 46, 12); \
	G256(G0, G1, G2, G3, 58, 22); \
	G256(G0, G3, G2, G1, 32, 32); \
	KS256(r + 1);

#define IG8256(r) \
	IG256(G0, G3, G2, G1, 32, 32); \
	IG256(G0, G1, G2, G3, 58, 22); \
	IG256(G0, G3, G2, G1, 46, 12); \
	IG256(G0, G1, G2, G3, 25, 33); \
	IKS256(r); \
	IG256(G0, G3, G2, G1, 5, 37); \
	IG256(G0, G1, G2, G3, 23, 40); \
	IG256(G0, G3, G2, G1, 52, 57); \
	IG256(G0, G1, G2, G3, 14, 16); \
	IKS256(r - 1);

#define IG512(G0, G1, G2, G3, G4, G5, G6, G7, C0, C1, C2, C3) \
	G7 = rotater64(G7 ^ G6, C3); \
	G6 -= G7; \
	G5 = rotater64(G5 ^ G4, C2); \
	G4 -= G5; \
	G3 = rotater64(G3 ^ G2, C1); \
	G2 -= G3; \
	G1 = rotater64(G1 ^ G0, C0); \
	G0 -= G1;

#define G512(G0, G1, G2, G3, G4, G5, G6, G7, C0, C1, C2, C3) \
	G0 += G1; \
	G1 = rotatel64(G1, C0) ^ G0; \
	G2 += G3; \
	G3 = rotatel64(G3, C1) ^ G2; \
	G4 += G5; \
	G5 = rotatel64(G5, C2) ^ G4; \
	G6 += G7; \
	G7 = rotatel64(G7, C3) ^ G6;

#define IKS512(r) \
	G0 -= m_rkey[(r + 1) % 9]; \
	G1 -= m_rkey[(r + 2) % 9]; \
	G2 -= m_rkey[(r + 3) % 9]; \
	G3 -= m_rkey[(r + 4) % 9]; \
	G4 -= m_rkey[(r + 5) % 9]; \
	G5 -= (m_rkey[(r + 6) % 9] + m_tweak[(r + 1) % 3]); \
	G6 -= (m_rkey[(r + 7) % 9] + m_tweak[(r + 2) % 3]); \
	G7 -= (m_rkey[(r + 8) % 9] + r + 1);

#define KS512(r) \
	G0 += m_rkey[(r + 1) % 9]; \
	G1 += m_rkey[(r + 2) % 9]; \
	G2 += m_rkey[(r + 3) % 9]; \
	G3 += m_rkey[(r + 4) % 9]; \
	G4 += m_rkey[(r + 5) % 9]; \
	G5 += m_rkey[(r + 6) % 9] + m_tweak[(r + 1) % 3]; \
	G6 += m_rkey[(r + 7) % 9] + m_tweak[(r + 2) % 3]; \
	G7 += m_rkey[(r + 8) % 9] + r + 1;

#define IG8512(r) \
	IG512(G6, G1, G0, G7, G2, G5, G4, G3, 8, 35, 56, 22); \
	IG512(G4, G1, G6, G3, G0, G5, G2, G7, 25, 29, 39, 43); \
	IG512(G2, G1, G4, G7, G6, G5, G0, G3, 13, 50, 10, 17); \
    IG512(G0, G1, G2, G3, G4, G5, G6, G7, 39, 30, 34, 24); \
	IKS512(r) \
	IG512(G6, G1, G0, G7, G2, G5, G4, G3, 44, 9, 54, 56); \
	IG512(G4, G1, G6, G3, G0, G5, G2, G7, 17, 49, 36, 39); \
	IG512(G2, G1, G4, G7, G6, G5, G0, G3, 33, 27, 14, 42); \
	IG512(G0, G1, G2, G3, G4, G5, G6, G7, 46, 36, 19, 37); \
	IKS512(r - 1)

#define G8512(r) \
	G512(G0, G1, G2, G3, G4, G5, G6, G7, 46, 36, 19, 37); \
	G512(G2, G1, G4, G7, G6, G5, G0, G3, 33, 27, 14, 42); \
	G512(G4, G1, G6, G3, G0, G5, G2, G7, 17, 49, 36, 39); \
	G512(G6, G1, G0, G7, G2, G5, G4, G3, 44, 9, 54, 56); \
	KS512(r) \
    G512(G0, G1, G2, G3, G4, G5, G6, G7, 39, 30, 34, 24); \
	G512(G2, G1, G4, G7, G6, G5, G0, G3, 13, 50, 10, 17); \
	G512(G4, G1, G6, G3, G0, G5, G2, G7, 25, 29, 39, 43); \
	G512(G6, G1, G0, G7, G2, G5, G4, G3, 8, 35, 56, 22); \
	KS512(r + 1)

#define IG1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, C1, C2, C3, C4, C5, C6, C7, C8) \
	G15 = rotater64(G15 ^ G14, C8); \
	G14 -= G15; \
	G13 = rotater64(G13 ^ G12, C7); \
	G12 -= G13; \
	G11 = rotater64(G11 ^ G10, C6); \
	G10 -= G11; \
	G9 = rotater64(G9 ^ G8, C5); \
	G8 -= G9; \
	G7 = rotater64(G7 ^ G6, C4); \
	G6 -= G7; \
	G5 = rotater64(G5 ^ G4, C3); \
	G4 -= G5; \
	G3 = rotater64(G3 ^ G2, C2); \
	G2 -= G3; \
	G1 = rotater64(G1 ^ G0, C1); \
	G0 -= G1;

#define G1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, C1, C2, C3, C4, C5, C6, C7, C8) \
	G0 += G1; \
	G1 = rotatel64(G1, C1) ^ G0; \
	G2 += G3; \
	G3 = rotatel64(G3, C2) ^ G2; \
	G4 += G5; \
	G5 = rotatel64(G5, C3) ^ G4; \
	G6 += G7; \
	G7 = rotatel64(G7, C4) ^ G6; \
	G8 += G9; \
	G9 = rotatel64(G9, C5) ^ G8; \
	G10 += G11; \
	G11 = rotatel64(G11, C6) ^ G10; \
	G12 += G13; \
	G13 = rotatel64(G13, C7) ^ G12; \
	G14 += G15; \
	G15 = rotatel64(G15, C8) ^ G14;

#define IKS1024(r) \
	G0 -= m_rkey[(r + 1) % 17]; \
	G1 -= m_rkey[(r + 2) % 17]; \
	G2 -= m_rkey[(r + 3) % 17]; \
	G3 -= m_rkey[(r + 4) % 17]; \
	G4 -= m_rkey[(r + 5) % 17]; \
	G5 -= m_rkey[(r + 6) % 17]; \
	G6 -= m_rkey[(r + 7) % 17]; \
	G7 -= m_rkey[(r + 8) % 17]; \
	G8 -= m_rkey[(r + 9) % 17]; \
	G9 -= m_rkey[(r + 10) % 17]; \
	G10 -= m_rkey[(r + 11) % 17]; \
	G11 -= m_rkey[(r + 12) % 17]; \
	G12 -= m_rkey[(r + 13) % 17]; \
	G13 -= (m_rkey[(r + 14) % 17] + m_tweak[(r + 1) % 3]); \
	G14 -= (m_rkey[(r + 15) % 17] + m_tweak[(r + 2) % 3]); \
	G15 -= (m_rkey[(r + 16) % 17] + r + 1);

#define KS1024(r) \
	G0 += m_rkey[(r + 1) % 17]; \
	G1 += m_rkey[(r + 2) % 17]; \
	G2 += m_rkey[(r + 3) % 17]; \
	G3 += m_rkey[(r + 4) % 17]; \
	G4 += m_rkey[(r + 5) % 17]; \
	G5 += m_rkey[(r + 6) % 17]; \
	G6 += m_rkey[(r + 7) % 17]; \
	G7 += m_rkey[(r + 8) % 17]; \
	G8 += m_rkey[(r + 9) % 17]; \
	G9 += m_rkey[(r + 10) % 17]; \
	G10 += m_rkey[(r + 11) % 17]; \
	G11 += m_rkey[(r + 12) % 17]; \
	G12 += m_rkey[(r + 13) % 17]; \
	G13 += m_rkey[(r + 14) % 17] + m_tweak[(r + 1) % 3]; \
	G14 += m_rkey[(r + 15) % 17] + m_tweak[(r + 2) % 3]; \
	G15 += m_rkey[(r + 16) % 17] + r + 1;

#define IG81024(r) \
	IG1024(G0, G15, G2, G11, G6, G13, G4, G9, G14, G1, G8, G5, G10, G3, G12, G7, 9, 48, 35, 52, 23, 31, 37, 20); \
	IG1024(G0, G7, G2, G5, G4, G3, G6, G1, G12, G15, G14, G13, G8, G11, G10, G9, 31, 44, 47, 46, 19, 42, 44, 25); \
	IG1024(G0, G9, G2, G13, G6, G11, G4, G15, G10, G7, G12, G3, G14, G5, G8, G1, 16, 34, 56, 51, 4, 53, 42, 41); \
	IG1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, 41, 9, 37, 31, 12, 47, 44, 30); \
	IKS1024(r); \
	IG1024(G0, G15, G2, G11, G6, G13, G4, G9, G14, G1, G8, G5, G10, G3, G12, G7, 5, 20, 48, 41, 47, 28, 16, 25); \
	IG1024(G0, G7, G2, G5, G4, G3, G6, G1, G12, G15, G14, G13, G8, G11, G10, G9, 33, 4, 51, 13, 34, 41, 59, 17); \
	IG1024(G0, G9, G2, G13, G6, G11, G4, G15, G10, G7, G12, G3, G14, G5, G8, G1, 38, 19, 10, 55, 49, 18, 23, 52); \
	IG1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, 24, 13, 8, 47, 8, 17, 22, 37); \
	IKS1024(r - 1);

#define G81024(r) \
	G1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, 24, 13, 8, 47, 8, 17, 22, 37); \
	G1024(G0, G9, G2, G13, G6, G11, G4, G15, G10, G7, G12, G3, G14, G5, G8, G1, 38, 19, 10, 55, 49, 18, 23, 52); \
	G1024(G0, G7, G2, G5, G4, G3, G6, G1, G12, G15, G14, G13, G8, G11, G10, G9, 33, 4, 51, 13, 34, 41, 59, 17); \
	G1024(G0, G15, G2, G11, G6, G13, G4, G9, G14, G1, G8, G5, G10, G3, G12, G7, 5, 20, 48, 41, 47, 28, 16, 25); \
	KS1024(r); \
	G1024(G0, G1, G2, G3, G4, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G15, 41, 9, 37, 31, 12, 47, 44, 30); \
	G1024(G0, G9, G2, G13, G6, G11, G4, G15, G10, G7, G12, G3, G14, G5, G8, G1, 16, 34, 56, 51, 4, 53, 42, 41); \
	G1024(G0, G7, G2, G5, G4, G3, G6, G1, G12, G15, G14, G13, G8, G11, G10, G9, 31, 44, 47, 46, 19, 42, 44, 25); \
	G1024(G0, G15, G2, G11, G6, G13, G4, G9, G14, G1, G8, G5, G10, G3, G12, G7, 9, 48, 35, 52, 23, 31, 37, 20); \
	KS1024(r + 1);

ANONYMOUS_NAMESPACE_END

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

NAMESPACE_BEGIN(CryptoPP)

void Threefish::Base::UncheckedSetKey(const byte *key, unsigned int keylen, const NameValuePairs &params)
{
    switch (keylen)
    {
    case 32:  // 256
		m_rkey.New(5);
		m_wspace.New(4);
        m_blocksize = 32;

		GetUserKey(LITTLE_ENDIAN_ORDER, m_rkey.begin(), 4, key, 32);
		m_rkey[4] = W64LIT(0x1BD11BDAA9FC1A22) ^ m_rkey[0] ^ m_rkey[1] ^ m_rkey[2] ^ m_rkey[3];
        break;
    case 64:  // 512
		m_rkey.New(9);
		m_wspace.New(8);
        m_blocksize = 64;

		GetUserKey(LITTLE_ENDIAN_ORDER, m_rkey.begin(), 8, key, 64);
		m_rkey[8] = W64LIT(0x1BD11BDAA9FC1A22) ^ m_rkey[0] ^ m_rkey[1] ^ m_rkey[2] ^ m_rkey[3] ^ m_rkey[4] ^
			m_rkey[5] ^ m_rkey[6] ^ m_rkey[7];
        break;
    case 128:  // 128
		m_rkey.New(17);
		m_wspace.New(16);
        m_blocksize = 128;

		GetUserKey(LITTLE_ENDIAN_ORDER, m_rkey.begin(), 16, key, 128);
		m_rkey[16] = W64LIT(0x1BD11BDAA9FC1A22) ^ m_rkey[0] ^ m_rkey[1] ^ m_rkey[2] ^ m_rkey[3] ^ m_rkey[4] ^
			m_rkey[5] ^ m_rkey[6] ^ m_rkey[7] ^ m_rkey[8] ^ m_rkey[9] ^ m_rkey[10] ^ m_rkey[11] ^ m_rkey[12] ^
			m_rkey[13] ^ m_rkey[14] ^ m_rkey[15];
        break;
    default:
        CRYPTOPP_ASSERT(0);
    }

	m_tweak.New(3);
	ConstByteArrayParameter t;
	if (params.GetValue(Name::Tweak(), t))
	{
		CRYPTOPP_ASSERT(t.size() == 16);
		::memcpy(m_tweak.begin(), t.begin(), 16);
		m_tweak[2] = m_tweak[0] + m_tweak[1];
	}
	else
	{
		::memset(m_tweak.begin(), 0x00, 24);
	}
}

void Threefish::Base::ProcessAndXorBlock(const byte *inBlock, const byte *xorBlock, byte *outBlock) const
{
	switch(m_blocksize)
	{
	case 32:
		ProcessAndXorBlock_256(inBlock, xorBlock, outBlock);
		break;
	case 64:
		ProcessAndXorBlock_512(inBlock, xorBlock, outBlock);
		break;
	case 128:
		ProcessAndXorBlock_1024(inBlock, xorBlock, outBlock);
		break;
	default:
		CRYPTOPP_ASSERT(0);
	}
}

void Threefish::Base::ProcessAndXorBlock_256(const byte *inBlock, const byte *xorBlock, byte *outBlock) const
{
	word64 &G0=m_wspace[0], &G1=m_wspace[1], &G2=m_wspace[2], &G3=m_wspace[3];

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef GetBlock<word64, LittleEndian, false> InBlock;
	InBlock iblk(inBlock);
	iblk(G0)(G1)(G2)(G3);

	if (IsForwardTransformation())
	{
		G0 += m_rkey[0]; G1 += m_rkey[1]; G2 += m_rkey[2]; G3 += m_rkey[3];
		G1 += m_tweak[0]; G2 += m_tweak[1];

		G8256(0); G8256(2); G8256(4); G8256(6); G8256(8);
		G8256(10); G8256(12); G8256(14); G8256(16);
	}
	else
	{
		G0 -= m_rkey[3]; G1 -= m_rkey[4]; G2 -= m_rkey[0]; G3 -= m_rkey[1];
		G1 -= m_tweak[0]; G2 -= m_tweak[1]; G3 -= 18;

		IG8256(16); IG8256(14); IG8256(12); IG8256(10);
		IG8256(8); IG8256(6); IG8256(4); IG8256(2); IG8256(0);
	}

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef PutBlock<word64, LittleEndian, false> OutBlock;
	OutBlock oblk(xorBlock, outBlock);
	oblk(G0)(G1)(G2)(G3);
}

void Threefish::Base::ProcessAndXorBlock_512(const byte *inBlock, const byte *xorBlock, byte *outBlock) const
{
	word64 &G0=m_wspace[0],   &G1=m_wspace[1],   &G2=m_wspace[2],   &G3=m_wspace[3];
	word64 &G4=m_wspace[4],   &G5=m_wspace[5],   &G6=m_wspace[6],   &G7=m_wspace[7];

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef GetBlock<word64, LittleEndian, false> InBlock;
	InBlock iblk(inBlock);
	iblk(G0)(G1)(G2)(G3)(G4)(G5)(G6)(G7);

	if (IsForwardTransformation())
	{
#if CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE && 0
		const bool s_sse2 = HasSSE2();
		if (s_sse2)
		{
			const word64 *ky = m_rkey.begin(), *tw = m_tweak.begin();
			word64 *ws = m_wspace.begin();

			// 15 SSE instructions
			_mm_store_si128((__m128i*)ws,
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)ws),
					_mm_load_si128((const __m128i*)ky)));
			_mm_store_si128((__m128i*)(ws+2),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+2)),
					_mm_load_si128((const __m128i*)(ky+2))));
			_mm_store_si128((__m128i*)(ws+4),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+4)),
					_mm_load_si128((const __m128i*)(ky+4))));
			_mm_store_si128((__m128i*)(ws+6),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+6)),
					_mm_load_si128((const __m128i*)(ky+6))));
			_mm_storeu_si128((__m128i*)(ws+5),
				_mm_add_epi64(
					_mm_loadu_si128((const __m128i*)(ws+5)),
					_mm_load_si128((const __m128i*)(tw))));
		}
#endif
		{
			// 34 integer instructions total
			G0 += m_rkey[0]; G1 += m_rkey[1]; G2 += m_rkey[2]; G3 += m_rkey[3];
			G4 += m_rkey[4]; G5 += m_rkey[5]; G6 += m_rkey[6]; G7 += m_rkey[7];
			G5 += m_tweak[0]; G6 += m_tweak[1];
		}

		G8512(0); G8512(2); G8512(4); G8512(6); G8512(8);
		G8512(10); G8512(12); G8512(14); G8512(16);
	}
	else
	{
		G0 -= m_rkey[0]; G1 -= m_rkey[1]; G2 -= m_rkey[2]; G3 -= m_rkey[3];
		G4 -= m_rkey[4]; G5 -= m_rkey[5]; G6 -= m_rkey[6]; G7 -= m_rkey[7];
		G5 -= m_tweak[0]; G6 -= m_tweak[1];	G7 -= 18;

		IG8512(16); IG8512(14); IG8512(12); IG8512(10);
		IG8512(8); IG8512(6); IG8512(4); IG8512(2);	IG8512(0);
	}

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef PutBlock<word64, LittleEndian, false> OutBlock;
	OutBlock oblk(xorBlock, outBlock);
	oblk(G0)(G1)(G2)(G3)(G4)(G5)(G6)(G7);
}

void Threefish::Base::ProcessAndXorBlock_1024(const byte *inBlock, const byte *xorBlock, byte *outBlock) const
{
	word64  &G0=m_wspace[0],   &G1=m_wspace[1],   &G2=m_wspace[2],   &G3=m_wspace[3];
	word64  &G4=m_wspace[4],   &G5=m_wspace[5],   &G6=m_wspace[6],   &G7=m_wspace[7];
	word64  &G8=m_wspace[8],   &G9=m_wspace[9],  &G10=m_wspace[10], &G11=m_wspace[11];
	word64 &G12=m_wspace[12], &G13=m_wspace[13], &G14=m_wspace[14], &G15=m_wspace[15];

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef GetBlock<word64, LittleEndian, false> InBlock;
	InBlock iblk(inBlock);
	iblk(G0)(G1)(G2)(G3)(G4)(G5)(G6)(G7)(G8)(G9)(G10)(G11)(G12)(G13)(G14)(G15);

	if (IsForwardTransformation())
	{
#if CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE && 0
		const bool s_sse2 = HasSSE2();
		if (s_sse2)
		{
			const word64 *ky = m_rkey.begin(), *tw = m_tweak.begin();
			word64 *ws = m_wspace.begin();

			_mm_store_si128((__m128i*)ws,
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)ws),
					_mm_load_si128((const __m128i*)ky)));
			_mm_store_si128((__m128i*)(ws+2),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+2)),
					_mm_load_si128((const __m128i*)(ky+2))));
			_mm_store_si128((__m128i*)(ws+4),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+4)),
					_mm_load_si128((const __m128i*)(ky+4))));
			_mm_store_si128((__m128i*)(ws+6),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+6)),
					_mm_load_si128((const __m128i*)(ky+6))));
			_mm_store_si128((__m128i*)(ws+8),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+8)),
					_mm_load_si128((const __m128i*)(ky+8))));
			_mm_store_si128((__m128i*)(ws+10),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+10)),
					_mm_load_si128((const __m128i*)(ky+10))));
			_mm_store_si128((__m128i*)(ws+12),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+12)),
					_mm_load_si128((const __m128i*)(ky+12))));
			_mm_store_si128((__m128i*)(ws+14),
				_mm_add_epi64(
					_mm_load_si128((const __m128i*)(ws+14)),
					_mm_load_si128((const __m128i*)(ky+14))));
			_mm_storeu_si128((__m128i*)(ws+13),
				_mm_add_epi64(
					_mm_loadu_si128((const __m128i*)(ws+13)),
					_mm_load_si128((const __m128i*)(tw))));
		}
		else
#endif
		{
			G0 += m_rkey[0]; G1 += m_rkey[1]; G2 += m_rkey[2]; G3 += m_rkey[3];
			G4 += m_rkey[4]; G5 += m_rkey[5]; G6 += m_rkey[6]; G7 += m_rkey[7];
			G8 += m_rkey[8]; G9 += m_rkey[9]; G10 += m_rkey[10]; G11 += m_rkey[11];
			G12 += m_rkey[12]; G13 += m_rkey[13]; G14 += m_rkey[14]; G15 += m_rkey[15];
			G13 += m_tweak[0]; G14 += m_tweak[1];
		}

		G81024(0); G81024(2); G81024(4); G81024(6); G81024(8);
		G81024(10); G81024(12); G81024(14); G81024(16); G81024(18);
	}
	else
	{
		G0 -= m_rkey[3]; G1 -= m_rkey[4]; G2 -= m_rkey[5]; G3 -= m_rkey[6];
		G4 -= m_rkey[7]; G5 -= m_rkey[8]; G6 -= m_rkey[9]; G7 -= m_rkey[10];
		G8 -= m_rkey[11]; G9 -= m_rkey[12]; G10 -= m_rkey[13]; G11 -= m_rkey[14];
		G12 -= m_rkey[15]; G13 -= m_rkey[16]; G14 -= m_rkey[0]; G15 -= m_rkey[1];
		G13 -= m_tweak[2]; G14 -= m_tweak[0]; G15 -= 20;

		IG81024(18); IG81024(16); IG81024(14); IG81024(12);	IG81024(10);
		IG81024(8); IG81024(6); IG81024(4);	IG81024(2); IG81024(0);
	}

	// Reverse bytes on BigEndian; Align pointer on LittleEndian
	typedef PutBlock<word64, LittleEndian, false> OutBlock;
	OutBlock oblk(xorBlock, outBlock);
	oblk(G0)(G1)(G2)(G3)(G4)(G5)(G6)(G7)(G8)(G9)(G10)(G11)(G12)(G13)(G14)(G15);
}

NAMESPACE_END