#include "SkBlitRow.h"
#include "SkBlitMask.h"
#include "SkColorPriv.h"
#include "SkDither.h"

#if defined ( __mips_dsp)
static void S32_D565_Blend_mips(uint16_t* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src, int count,
                                U8CPU alpha, int /*x*/, int /*y*/) {
    register uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0;
    register uint32_t s0 = 0, s1 = 0, s2 = 0, s4 = 0, s5 = 0, s6 = 0;
    alpha +=1;
    if (count >= 2) {
        asm volatile (
            ".set noreorder                          \n\t"
            "sll             %[s4], %[alpha], 8      \n\t"  // s4 = mult vector
            "or              %[s4], %[s4], %[alpha]  \n\t"
            "repl.ph         %[s5], 0x1f             \n\t"
            "repl.ph         %[s6], 0x3f             \n\t"
            "1:                                      \n\t"
            "lw              %[s2], 0(%[src])        \n\t"
            "lw              %[s1], 4(%[src])        \n\t"
#if defined (SK_CPU_LENDIAN)
            "lwr             %[s0], 0(%[dst])        \n\t"
            "lwl             %[s0], 3(%[dst])        \n\t"
            "and             %[t1], %[s0], %[s5]     \n\t"  // t1 = 0|b1|0|b2
            "shra.ph         %[t0], %[s0], 5         \n\t"
            "and             %[t2], %[t0], %[s6]     \n\t"  // t2 = 0|g1|0|g2
#if __mips_dspr2
            "shrl.ph         %[t3], %[s0], 11        \n\t"
#else
            "shra.ph         %[t0], %[s0], 11        \n\t"  // Get two reds from two Dst pixels
            "and             %[t3], %[t0], %[s5]     \n\t"
#endif // (__mips_dspr2)
            "precrq.ph.w     %[t0], %[s1], %[s2]     \n\t"
            "shrl.qb         %[t5], %[t0], 3         \n\t"
            "and             %[t4], %[t5], %[s5]     \n\t"  // t4 = 0|b1|0|b2
            "ins             %[s2], %[s1], 16, 16    \n\t"
            "preceu.ph.qbra  %[t0], %[s2]            \n\t"
            "shrl.qb         %[t6], %[t0], 3         \n\t"  // t6 = 0|r1|0|r2
#if __mips_dspr2
            "shrl.ph         %[t5], %[s2], 10        \n\t"
#else
            "shra.ph         %[t0], %[s2], 10        \n\t"
            "and             %[t5], %[t0], %[s6]     \n\t"  // t5 has got two greens from src
#endif  // (__mips_dspr2)
#else  // SK_CPU_LENDIAN
            "lwl             %[s0], 0(%[dst])        \n\t"
            "lwr             %[s0], 3(%[dst])        \n\t"
            "and             %[t3], %[s0], %[s5]     \n\t"  // t3 = 0|r1|0|r2
            "shra.ph         %[t0], %[s0], 5         \n\t"
            "and             %[t2], %[t0], %[s6]     \n\t"  // t2 = 0|g1|0|g2
#ifdef __mips_dspr2
            "shrl.ph         %[t1], %[s0], 11        \n\t"
#else
            "shra.ph         %[t0], %[s0], 11        \n\t"  // Get two blues from two Dst pixels
            "and             %[t1], %[t0], %[s5]     \n\t"
#endif  // (__mips_dspr2)
            "precrq.ph.w     %[t0], %[s1], %[s2]     \n\t"
            "shrl.qb         %[t4], %[t0], 2         \n\t"
            "and             %[t5], %[t4], %[s6]     \n\t"  // t5 = 0|g1|0|g2
            "ins             %[s2], %[s1], 16, 16    \n\t"
            "shra.ph         %[t4], %[t0], 11        \n\t"  // t6 = 0|r1|0|r2
            "and             %[t6], %[t4], %[s5]     \n\t"
#ifdef __mips_dspr2
            "shrl.ph         %[t4], %[s2], 11        \n\t"
#else
            "shra.ph         %[t0], %[s2], 11        \n\t"
            "and             %[t4], %[t0], %[s5]     \n\t"  // t4 has two blues from src
#endif // (__mips_dspr2)
#endif // SK_CPU_LENDIAN
            "subu.qb         %[t4], %[t4], %[t1]     \n\t"
            "subu.qb         %[t5], %[t5], %[t2]     \n\t"
            "subu.qb         %[t6], %[t6], %[t3]     \n\t"
            "muleu_s.ph.qbr  %[t4], %[s4], %[t4]     \n\t"
            "muleu_s.ph.qbr  %[t5], %[s4], %[t5]     \n\t"
            "muleu_s.ph.qbr  %[t6], %[s4], %[t6]     \n\t"
            "addiu           %[count], %[count], -2  \n\t"
            "addiu           %[src], %[src], 8       \n\t"
            "shra.ph         %[t4], %[t4], 8         \n\t"
            "shra.ph         %[t5], %[t5], 8         \n\t"
            "shra.ph         %[t6], %[t6], 8         \n\t"
            "addu.qb         %[t4], %[t4], %[t1]     \n\t"
            "addu.qb         %[t5], %[t5], %[t2]     \n\t"
            "addu.qb         %[t6], %[t6], %[t3]     \n\t"
            "andi            %[s0], %[t4], 0xffff    \n\t"
            "andi            %[t0], %[t5], 0xffff    \n\t"
            "sll             %[t0], %[t0], 0x5       \n\t"
            "or              %[s0], %[s0], %[t0]     \n\t"
            "sll             %[t0], %[t6], 0xb       \n\t"
            "or              %[t0], %[t0], %[s0]     \n\t"
            "sh              %[t0], 0(%[dst])        \n\t"
            "srl             %[s1], %[t4], 16        \n\t"
            "srl             %[t0], %[t5], 16        \n\t"
            "sll             %[t5], %[t0], 5         \n\t"
            "or              %[t0], %[t5], %[s1]     \n\t"
            "srl             %[s0], %[t6], 16        \n\t"
            "sll             %[s2], %[s0], 0xb       \n\t"
            "or              %[s1], %[s2], %[t0]     \n\t"
            "sh              %[s1], 2(%[dst])        \n\t"
            "bge             %[count], 2, 1b         \n\t"
            " addiu          %[dst], %[dst], 4       \n\t"
            ".set reorder                            \n\t"
            :[t0]"=&r"(t0), [t1]"=&r"(t1), [t2]"=&r"(t2), [t3]"=&r"(t3), [t4]"=&r"(t4),
             [t5]"=&r"(t5), [t6]"=&r"(t6), [s0]"=&r"(s0), [s1]"=&r"(s1), [s2]"=&r"(s2),
             [s4]"=&r"(s4), [s5]"=&r"(s5), [s6]"=&r"(s6),
             [count]"+r"(count), [dst]"+r"(dst), [src]"+r"(src)
            :[alpha]"r"(alpha)
            );
    }
    if (count == 1) {
        SkPMColor c = *src++;
        SkPMColorAssert(c);
        SkASSERT(SkGetPackedA32(c) == 255);
        uint16_t d = *dst;
        *dst++ = SkPackRGB16( SkAlphaBlend(SkPacked32ToR16(c), SkGetPackedR16(d), alpha),
                              SkAlphaBlend(SkPacked32ToG16(c), SkGetPackedG16(d), alpha),
                              SkAlphaBlend(SkPacked32ToB16(c), SkGetPackedB16(d), alpha));
    }
}

static void S32A_D565_Opaque_Dither_mips(uint16_t* __restrict__ dst,
                                         const SkPMColor* __restrict__ src,
                                         int count, U8CPU alpha, int x, int y) {
    asm volatile (
        "pref  0, 0(%0)   \n\t"
        "pref  1, 0(%1)   \n\t"
        "pref  0, 32(%0)  \n\t"
        "pref  1, 32(%1)  \n\t"
        ::"r"(src), "r"(dst));
    register int32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0;
    register int32_t t7 = 0, t8 = 0, t9 = 0;
    register int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    const uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
    if (count >= 2) {
        asm volatile (
            ".set noreorder                                \n\t"
            "li              %[s1], 0x01010101             \n\t"
            "li              %[s2], -2017                  \n\t"
            "1:                                            \n\t"
            "bnez            %[s3], 4f                     \n\t"
            "li              %[s3], 2                      \n\t"
            "pref            0, 64(%[src])                 \n\t"
            "pref            1, 64(%[dst])                 \n\t"
            "4:                                            \n\t"
            "addiu           %[s3], %[s3], -1              \n\t"

            "lw              %[t1], 0(%[src])              \n\t"  // t1 = src1
            "andi            %[t3], %[x], 0x3              \n\t"
            "addiu           %[x], %[x], 1                 \n\t"
            "sll             %[t4], %[t3], 2               \n\t"
            "srav            %[t5], %[dither_scan], %[t4]  \n\t"
            "andi            %[t3], %[t5], 0xf             \n\t"  // t3 has mul op1
            "lw              %[t2], 4(%[src])              \n\t"  // t2 = src2
            "andi            %[t4], %[x], 0x3              \n\t"
            "sll             %[t5], %[t4], 2               \n\t"
            "srav            %[t6], %[dither_scan], %[t5]  \n\t"
            "addiu           %[x], %[x], 1                 \n\t"
            "ins             %[t3], %[t6], 8, 4            \n\t"  // t3 has 2 ops
#ifdef SK_CPU_BENDIAN
            "andi            %[t4], %[t1], 0xff            \n\t"
            "addiu           %[t0], %[t4], 1               \n\t"  // t0 = alpha + 1
            "andi            %[t4], %[t2], 0xff            \n\t"  // t4 = 0|0|0|a2
#else // SK_CPU_LENDIAN
            "srl             %[t4], %[t1], 24              \n\t"
            "addiu           %[t0], %[t4], 1               \n\t"  // t0 = alpha + 1
            "srl             %[t4], %[t2], 24              \n\t"  // t4 = 0|0|0|a2
#endif // SK_CPU_BENDIAN
            "addiu           %[t5], %[t4], 1               \n\t"  // t5 = alpha + 1
            "ins             %[t0], %[t5], 16, 16          \n\t"  // t0 has 2 (a+1)
            "muleu_s.ph.qbr  %[t4], %[t3], %[t0]           \n\t"
            "preceu.ph.qbla  %[t3], %[t4]                  \n\t"  // t3 = t4>>8
#ifdef SK_CPU_BENDIAN
            "srl             %[t4], %[t1], 24              \n\t"
            "srl             %[t5], %[t2], 24              \n\t"
            "ins             %[t4], %[t5], 16, 16          \n\t"
#else // SK_CPU_LENDIAN
            "andi            %[t4], %[t1], 0xff            \n\t"  // t4 = r
            "ins             %[t4], %[t2], 16, 8           \n\t"
#endif // SK_CPU_BENDIAN
            "shrl.qb         %[t5], %[t4], 5               \n\t"
            "subu.qb         %[t6], %[t3], %[t5]           \n\t"
            "addq.ph         %[t5], %[t6], %[t4]           \n\t"  // t5 = new sr
#ifdef SK_CPU_BENDIAN
            "ext             %[t4], %[t1], 16, 8           \n\t"
            "srl             %[t6], %[t2], 16              \n\t"
            "ins             %[t4], %[t6], 16, 8           \n\t"
#else // SK_CPU_LENDIAN
            "ext             %[t4], %[t1], 8, 8            \n\t"  // t4 = g
            "srl             %[t6], %[t2], 8               \n\t"
            "ins             %[t4], %[t6], 16, 8           \n\t"
#endif // SK_CPU_BENDIAN
            "shrl.qb         %[t6], %[t4], 6               \n\t"
            "shrl.qb         %[t7], %[t3], 1               \n\t"
            "subu.qb         %[t8], %[t7], %[t6]           \n\t"
            "addq.ph         %[t6], %[t8], %[t4]           \n\t"  // t6 = new sg
#ifdef SK_CPU_BENDIAN
            "ext             %[t4], %[t1], 8, 8            \n\t"
            "srl             %[t7], %[t2], 8               \n\t"
            "ins             %[t4], %[t7], 16, 8           \n\t"
#else // SK_CPU_LENDIAN
            "ext             %[t4], %[t1], 16, 8           \n\t"  // t4 = b
            "srl             %[t7], %[t2], 16              \n\t"
            "ins             %[t4], %[t7], 16, 8           \n\t"
#endif // SK_CPU_BENDIAN
            "shrl.qb         %[t7], %[t4], 5               \n\t"
            "subu.qb         %[t8], %[t3], %[t7]           \n\t"
            "addq.ph         %[t7], %[t8], %[t4]           \n\t"  // new sb
            "shll.ph         %[t4], %[t7], 2               \n\t"  // src expanded
            "andi            %[t9], %[t4], 0xffff          \n\t"  // sb1
            "srl             %[s0], %[t4], 16              \n\t"  // sb2
            "andi            %[t3], %[t6], 0xffff          \n\t"  // sg1
            "srl             %[t4], %[t6], 16              \n\t"  // sg2
            "andi            %[t6], %[t5], 0xffff          \n\t"  // sr1
            "srl             %[t7], %[t5], 16              \n\t"  // sr2
            "subq.ph         %[t5], %[s1], %[t0]           \n\t"
            "srl             %[t0], %[t5], 3               \n\t"  // (256-a)>>3
            "beqz            %[t1], 3f                     \n\t"
            "lhu             %[t5], 0(%[dst])              \n\t"
            "sll             %[t1], %[t6], 13              \n\t"  // sr1 << 13
            "or              %[t8], %[t9], %[t1]           \n\t"  // sr1|sb1
            "sll             %[t1], %[t3], 24              \n\t"  // sg1 << 24
            "or              %[t9], %[t1], %[t8]           \n\t"  // t9 = sr1|sb1|sg1
            "andi            %[t3], %[t5], 0x7e0           \n\t"
            "sll             %[t6], %[t3], 0x10            \n\t"
            "and             %[t8], %[s2], %[t5]           \n\t"
            "or              %[t5], %[t6], %[t8]           \n\t"  // dst_expanded
            "andi            %[t6], %[t0], 0xff            \n\t"
            "mul             %[t1], %[t6], %[t5]           \n\t"  // dst_expanded
            "addu            %[t5], %[t1], %[t9]           \n\t"
            "srl             %[t6], %[t5], 5               \n\t"
            "and             %[t5], %[s2], %[t6]           \n\t"  // ~greenmask
            "srl             %[t8], %[t6], 16              \n\t"
            "andi            %[t6], %[t8], 0x7e0           \n\t"
            "or              %[t1], %[t5], %[t6]           \n\t"
            "sh              %[t1], 0(%[dst])              \n\t"

            "3:                                            \n\t"
            "beqz            %[t2], 2f                     \n\t"
            "lhu             %[t5], 2(%[dst])              \n\t"
            "sll             %[t1], %[t7], 13              \n\t"  // sr2 << 13
            "or              %[t8], %[s0], %[t1]           \n\t"  // sb2|sr2
            "sll             %[t1], %[t4], 24              \n\t"  // sg2 << 24
            "or              %[t9], %[t1], %[t8]           \n\t"  // t9 = sr2|sb2|sg2
            "andi            %[t3], %[t5], 0x7e0           \n\t"
            "sll             %[t6], %[t3], 0x10            \n\t"
            "and             %[t8], %[s2], %[t5]           \n\t"
            "or              %[t5], %[t6], %[t8]           \n\t"  // dst_expanded
            "srl             %[t6], %[t0], 16              \n\t"  // (256-a)>>3
            "mul             %[t1], %[t6], %[t5]           \n\t"  // dst_expanded
            "addu            %[t5], %[t1], %[t9]           \n\t"
            "srl             %[t6], %[t5], 5               \n\t"
            "and             %[t5], %[s2], %[t6]           \n\t"  // ~greenmask
            "srl             %[t8], %[t6], 16              \n\t"
            "andi            %[t6], %[t8], 0x7e0           \n\t"
            "or              %[t1], %[t5], %[t6]           \n\t"
            "sh              %[t1], 2(%[dst])              \n\t"

            "2:                                            \n\t"
            "addiu           %[count], %[count], -2        \n\t"
            "addiu           %[src], %[src], 8             \n\t"
            "addiu           %[t1], %[count], -1           \n\t"
            "bgtz            %[t1], 1b                     \n\t"
            " addiu           %[dst], %[dst], 4            \n\t"
            ".set reorder                                  \n\t"
            : [src]"+r"(src), [count]"+r"(count), [dst]"+r"(dst), [x]"+r"(x),
              [t0]"+r"(t0), [t1]"=&r"(t1), [t2]"+r"(t2), [t3]"=&r"(t3), [t4]"=&r"(t4),
              [t5]"=&r"(t5), [t6]"+r"(t6), [t7]"+r"(t7),  [t8]"+r"(t8),  [t9]"+r"(t9),
              [s0]"+r"(s0), [s1]"=&r"(s1), [s2]"=&r"(s2), [s3]"=&r"(s3)
            : [dither_scan]"r"(dither_scan)
            );
    }
    if (count == 1) {
        SkPMColor c = *src++;
        SkPMColorAssert(c);
        if (c) {
            unsigned a = SkGetPackedA32(c);
            int d = SkAlphaMul(DITHER_VALUE(x), SkAlpha255To256(a));

            unsigned sr = SkGetPackedR32(c);
            unsigned sg = SkGetPackedG32(c);
            unsigned sb = SkGetPackedB32(c);
            sr = SkDITHER_R32_FOR_565(sr, d);
            sg = SkDITHER_G32_FOR_565(sg, d);
            sb = SkDITHER_B32_FOR_565(sb, d);

            uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
            uint32_t dst_expanded = SkExpand_rgb_16(*dst);
            dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
            // now src and dst expanded are in g:11 r:10 x:1 b:10
            *dst = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
        }
        dst += 1;
        DITHER_INC_X(x);
    }
}

#define S32A_D565_Opaque_Dither_PROC S32A_D565_Opaque_Dither_mips
#define S32_D565_Blend_PROC S32_D565_Blend_mips

#else // __mips_dsp

#define S32A_D565_Opaque_Dither_PROC NULL
#define S32_D565_Blend_PROC NULL
#define S32A_D565_Blend_PROC NULL

#endif // __mips_dsp

#if defined ( __mips_dsp)
static void S32_D565_Opaque_Dither_mips(uint16_t* __restrict__ dst,
                                        const SkPMColor* __restrict__ src,
                                        int count, U8CPU alpha, int x, int y) {
    uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
    register uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0;
    register uint32_t t6 = 0, t7 = 0, t8 = 0, t9 = 0, s0 = 0;
    int dither[4];
    int i;
    for(i = 0; i < 4; i++, x++) {
        dither[i] = (dither_scan >> ((x&3)<<2)) & 0xF;
    }

    asm volatile (
        ".set noreorder                          \n\t"
        "li              %[s0], 1                \n\t"

        "2:                                      \n\t"
        "beqz            %[count], 1f            \n\t"
        " nop                                    \n\t"
        "addiu           %[t0], %[count], -1     \n\t"
        "beqz            %[t0], 1f               \n\t"
        " nop                                    \n\t"
        "beqz            %[s0], 3f               \n\t"
        " nop                                    \n\t"
        "lw              %[t0], 0(%[dither])     \n\t"
        "lw              %[t1], 4(%[dither])     \n\t"
        "li              %[s0], 0                \n\t"
        "b               4f                      \n\t"
        " nop                                    \n\t"

        "3:                                      \n\t"
        "lw              %[t0], 8(%[dither])     \n\t"
        "lw              %[t1], 12(%[dither])    \n\t"
        "li              %[s0], 1                \n\t"

        "4:                                      \n\t"
        "sll             %[t2], %[t0], 16        \n\t"
        "or              %[t1], %[t2], %[t1]     \n\t"
        "lw              %[t0], 0(%[src])        \n\t"
        "lw              %[t2], 4(%[src])        \n\t"
#ifdef SK_CPU_BENDIAN // BE: r|g|b|a
        "precrq.ph.w     %[t3], %[t0], %[t2]     \n\t"  // t3 = r1|g1|r2|g2
        "preceu.ph.qbla  %[t4], %[t3]            \n\t"  // t4 = 0|r1|0|r2
        "preceu.ph.qbra  %[t5], %[t3]            \n\t"  // t5 = 0|g1|0|g2
#ifdef __mips_dspr2
        "append          %[t0], %[t2], 16        \n\t"
        "preceu.ph.qbla  %[t9], %[t0]            \n\t"  // t9 = 0|b1|0|b2
#else
        "sll            %[t6], %[t0], 16         \n\t"      // t6 = b1|a1|0|0
        "sll            %[t7], %[t2], 16         \n\t"      // t7 = b2|a2|0|0
        "precrq.ph.w    %[t8], %[t6], %[t7]      \n\t"      // t8 = b1|a1|b2|a2
        "preceu.ph.qbla %[t9], %[t8]             \n\t"      // t9 = 0|b1|0|b2       B
#endif
#else // SK_CPU_LENDIAN LE: a|b|g|r
        "precrq.ph.w     %[t3], %[t0], %[t2]     \n\t"  // t3 = a1|b1|a2|b2
        "preceu.ph.qbra  %[t9], %[t3]            \n\t"  // t9 = 0|b1|0|b2
#ifdef __mips_dspr2
        "append          %[t0], %[t2], 16        \n\t"
        "preceu.ph.qbra  %[t4], %[t0]            \n\t"  // 0|r1|0|r2
        "preceu.ph.qbla  %[t5], %[t0]            \n\t"  // 0|g1|0|g2
#else
        "sll            %[t6], %[t0], 16         \n\t"      // t6 = g1|r1|0|0
        "sll            %[t7], %[t2], 16         \n\t"      // t7 = g2|r2|0|0
        "precrq.ph.w    %[t8], %[t6], %[t7]      \n\t"      // t8 = g1|r1|g2|r2
        "preceu.ph.qbra %[t4], %[t8]             \n\t"      // t9 = 0|r1|0|r2       R
        "preceu.ph.qbla %[t5], %[t8]             \n\t"      // t5 = 0|g1|0|g2       G
#endif
#endif
        "addu.qb         %[t0], %[t4], %[t1]     \n\t"
        "shra.ph         %[t2], %[t4], 5         \n\t"
        "subu.qb         %[t3], %[t0], %[t2]     \n\t"
        "shra.ph         %[t6], %[t3], 3         \n\t"  // t6 = 0|r1|0|r2
        "addu.qb         %[t0], %[t9], %[t1]     \n\t"
        "shra.ph         %[t2], %[t9], 5         \n\t"
        "subu.qb         %[t3], %[t0], %[t2]     \n\t"
        "shra.ph         %[t7], %[t3], 3         \n\t"  // t7 = 0|b1|0|b2

        "shra.ph         %[t0], %[t1], 1         \n\t"  // d >> 1
        "shra.ph         %[t2], %[t5], 6         \n\t"  // g >> 6
        "addu.qb         %[t3], %[t5], %[t0]     \n\t"
        "subu.qb         %[t4], %[t3], %[t2]     \n\t"
        "shra.ph         %[t8], %[t4], 2         \n\t"  // t8 = 0|g1|0|g2

        "precrq.ph.w     %[t0], %[t6], %[t7]     \n\t"  // t0 = 0|r1|0|b1
#ifdef __mips_dspr2
        "append          %[t6], %[t7], 16        \n\t"
#else
        "sll            %[t6], %[t6], 16         \n\t"
        "sll            %[t2], %[t7], 16         \n\t"
        "precrq.ph.w    %[t6], %[t6], %[t2]      \n\t"      // 0|r2|0|b2 calculated
#endif
        "sra             %[t4], %[t8], 16        \n\t"  // t4 = 0|0|0|g1
        "andi            %[t5], %[t8], 0xFF      \n\t"  // t5 = 0|0|0|g2

        "sll             %[t7], %[t4], 5         \n\t"  // g1 on place
        "sra             %[t8], %[t0], 5         \n\t"  // r1 on place
        "or              %[t9], %[t7], %[t8]     \n\t"
        "or              %[t3], %[t9], %[t0]     \n\t"
        "andi            %[t4], %[t3], 0xFFFF    \n\t"

        "sll             %[t7], %[t5], 5         \n\t"  // g2 on place
        "sra             %[t8], %[t6], 5         \n\t"  // r2 on place
        "or              %[t9], %[t7], %[t8]     \n\t"
        "or              %[t3], %[t9], %[t6]     \n\t"
        "and             %[t7], %[t3], 0xFFFF    \n\t"

        "sh              %[t4], 0(%[dst])        \n\t"
        "sh              %[t7], 2(%[dst])        \n\t"

        "addiu           %[count], %[count], -2  \n\t"
        "addiu           %[src], %[src], 8       \n\t"
        "b               2b                      \n\t"
        " addiu           %[dst], %[dst], 4      \n\t"
        "1:                                      \n\t"
        ".set reorder                            \n\t"
        : [dst] "+r" (dst), [src] "+r" (src), [count] "+r" (count), [x] "+r" (x),
          [t0] "=&r" (t0), [t1] "+r" (t1), [t2] "+r" (t2), [t3] "+r" (t3),
          [t4] "+r" (t4), [t5] "+r" (t5), [t6] "+r" (t6),[t7] "+r" (t7), [t8] "+r" (t8),
          [t9] "+r" (t9), [s0] "=&r" (s0)
        : [dither] "r" (dither)
    );
    if (count == 1)
     {
            SkPMColor c = *src++;
            SkPMColorAssert(c); // only if DEBUG is turned on
            SkASSERT(SkGetPackedA32(c) == 255);
            unsigned dither = DITHER_VALUE(x);
            *dst++ = SkDitherRGB32To565(c, dither);
     }
}

static void S32_D565_Blend_Dither_mips(uint16_t* dst,
                                       const SkPMColor* src,
                                       int count, U8CPU alpha, int x, int y) {
    register int32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0;
    register int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    register int x1 = 0;
    register uint32_t sc_mul = 0;
    register uint32_t sc_add = 0;

#ifdef ENABLE_DITHER_MATRIX_4X4
    const uint8_t* dither_scan = gDitherMatrix_3Bit_4X4[(y) & 3];
#else // ENABLE_DITHER_MATRIX_4X4
    const uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
#endif // ENABLE_DITHER_MATRIX_4X4

    int dither[4];
    for(int i=0; i<4; i++) {
        dither[i] = (dither_scan >> ((x&3)<<2)) & 0xF;
        x+=1;
    }
    alpha +=1;
    __asm__ __volatile__(
        ".set noreorder                            \n\t"
        "li              %[t0], 0x100              \n\t"
        "subu            %[t0], %[t0], %[alpha]    \n\t"
        "replv.ph        %[sc_mul], %[alpha]       \n\t"  // sc_mul = 0|scale|0|scale
        "beqz            %[alpha], 1f              \n\t"
        " nop                                      \n\t"
        "replv.qb        %[sc_add], %[t0]          \n\t"  // sc_add = 0|diff|0|diff
        "b               2f                        \n\t"
        " nop                                      \n\t"

        "1:                                        \n\t"
        "replv.qb        %[sc_add], %[alpha]       \n\t"

        "2:                                        \n\t"
        "addiu           %[t2], %[count], -1       \n\t"
        "blez            %[t2], 3f                 \n\t"
        " nop                                      \n\t"
        "lw              %[s0], 0(%[src])          \n\t"
        "lw              %[s1], 4(%[src])          \n\t"

        "bnez            %[x1], 4f                 \n\t"
        " nop                                      \n\t"
        "lw              %[t0], 0(%[dither])       \n\t"
        "lw              %[t1], 4(%[dither])       \n\t"
        "li              %[x1], 1                  \n\t"
        "b               5f                        \n\t"
        " nop                                      \n\t"

        "4:                                        \n\t"
        "lw              %[t0], 8(%[dither])       \n\t"
        "lw              %[t1], 12(%[dither])      \n\t"
        "li              %[x1], 0                  \n\t"

        "5:                                        \n\t"
        "sll             %[t3], %[t0], 7           \n\t"
        "sll             %[t4], %[t1], 7           \n\t"
#ifdef __mips_dspr2
        "append          %[t0], %[t1], 16          \n\t"
#else
        "sll             %[t0], %[t0], 8           \n\t"  // t1 = dither1 << 8;
        "sll             %[t2], %[t1], 8           \n\t"  // t2 = dither2 << 8;
        "precrq.qb.ph    %[t0], %[t0], %[t2]       \n\t"  // t0 = 0|dither1|0|dither2
#endif
        "precrq.qb.ph    %[t1], %[t3], %[t4]       \n\t"  // t1 = 0|dither3|0|dither4
#ifdef SK_CPU_BENDIAN  // BE: r|g|b|a
        "precrq.qb.ph    %[t4], %[s0], %[s1]       \n\t"  // t4 = 0xR1B1R2B2
        "sll             %[t5], %[s0], 8           \n\t"  // t5 = c1 << 8
        "sll             %[t6], %[s1], 8           \n\t"  // t6 = c2 << 8
        "precrq.qb.ph    %[t6], %[t5], %[t6]       \n\t"  // t6 = 0xG1A1G2A2

        "preceu.ph.qbra  %[t5], %[t4]              \n\t"  // t5 = 0|b1|0|b2
        "preceu.ph.qbla  %[t4], %[t4]              \n\t"  // t4 = 0|r1|0|r2
        "preceu.ph.qbla  %[t6], %[t6]              \n\t"  // t6 = 0|g1|0|g2
#else // SK_CPU_LENDIAN LE: a|b|g|r
        "sll             %[t5], %[s0], 8           \n\t"
        "sll             %[t6], %[s1], 8           \n\t"
        "precrq.qb.ph    %[t4], %[t5], %[t6]       \n\t"  // t4 = 0xB1R1B2R2
        "precrq.qb.ph    %[t6], %[s0], %[s1]       \n\t"  // t6 = 0xA1G1A2G2
        "preceu.ph.qbla  %[t5], %[t4]              \n\t"  // t5 = 0|b1|0|b2
        "preceu.ph.qbra  %[t4], %[t4]              \n\t"  // t4 = 0|r1|0|r2
        "preceu.ph.qbra  %[t6], %[t6]              \n\t"  // t6 = 0|g1|0|g2
#endif // SK_CPU_BENDIAN
        "lh              %[t2], 0(%[dst])          \n\t"
        "lh              %[s1], 2(%[dst])          \n\t"
#ifdef __mips_dspr2
        "append          %[t2], %[s1], 16          \n\t"
#else
        "sll             %[s1], %[s1], 16        \n\t"
        "packrl.ph       %[t2], %[t2], %[s1]      \n\t"  // t2 = dst1|dst1|dst2|dst2
#endif
        "shra.ph         %[s1], %[t2], 11          \n\t"  // r1,r2 = d1,d2>>11
        "and             %[s1], %[s1], 0x1F001F    \n\t"  // b = d & 0x1F001F
        "shra.ph         %[s2], %[t2], 5           \n\t"  // g1,g2 = d>>5 & 0x3F
        "and             %[s2], %[s2], 0x3F003F    \n\t"  // g = g & 0x3F003F
        "and             %[s3], %[t2], 0x1F001F    \n\t"  // b = d & 0x1F001F

        "shrl.qb         %[t3], %[t4], 5           \n\t"  // r1 >> 5 ^ r2 >> 5
        "addu.qb         %[t4], %[t4], %[t0]       \n\t"  // r + d
        "subu.qb         %[t4], %[t4], %[t3]       \n\t"  // r + d - (r>>5)
        "shrl.qb         %[t4], %[t4], 3           \n\t"  // >>3

        "shrl.qb         %[t3], %[t5], 5           \n\t"  // b1 >> 5 ^ b2 >> 5
        "addu.qb         %[t5], %[t5], %[t0]       \n\t"  // b + d
        "subu.qb         %[t5], %[t5], %[t3]       \n\t"  // b + d - (b>>5)
        "shrl.qb         %[t5], %[t5], 3           \n\t"  // >>3

        "shrl.qb         %[t3], %[t6], 6           \n\t"  // g >> 6
        "addu.qb         %[t6], %[t6], %[t1]       \n\t"
        "subu.qb         %[t6], %[t6], %[t3]       \n\t"
        "shrl.qb         %[t6], %[t6], 2           \n\t"

        "cmpu.lt.qb      %[t4], %[s1]              \n\t"
        "pick.qb         %[s0], %[sc_add], $0      \n\t"
        "addu.qb         %[s0], %[s0], %[s1]       \n\t"
        "subu.qb         %[t4], %[t4], %[s1]       \n\t"  // p1 - r
        "muleu_s.ph.qbl  %[t0], %[t4], %[sc_mul]   \n\t"
        "muleu_s.ph.qbr  %[t1], %[t4], %[sc_mul]   \n\t"
        "precrq.qb.ph    %[t4], %[t0], %[t1]       \n\t"
        "addu.qb         %[t4], %[t4], %[s0]       \n\t"  // t4 = 0xAABBGGRR or 0xRRGGBBAA
        "cmpu.lt.qb      %[t5], %[s3]              \n\t"
        "pick.qb         %[s0], %[sc_add], $0      \n\t"
        "addu.qb         %[s0], %[s0], %[s3]       \n\t"
        "subu.qb         %[t5], %[t5], %[s3]       \n\t"  // p2 - b
        "muleu_s.ph.qbl  %[t0], %[t5], %[sc_mul]   \n\t"
        "muleu_s.ph.qbr  %[t1], %[t5], %[sc_mul]   \n\t"
        "precrq.qb.ph    %[t5], %[t0], %[t1]       \n\t"
        "addu.qb         %[t5], %[t5], %[s0]       \n\t"  // t5 = 0xAABBGGRR or 0xRRGGBBAA

        "cmpu.lt.qb      %[t6], %[s2]              \n\t"
        "pick.qb         %[s0], %[sc_add], $0      \n\t"
        "addu.qb         %[s0], %[s0], %[s2]       \n\t"
        "subu.qb         %[t6], %[t6], %[s2]       \n\t"  // p3 - g
        "muleu_s.ph.qbl  %[t0], %[t6], %[sc_mul]   \n\t"
        "muleu_s.ph.qbr  %[t1], %[t6], %[sc_mul]   \n\t"
        "precrq.qb.ph    %[t6], %[t0], %[t1]       \n\t"
        "addu.qb         %[t6], %[t6], %[s0]       \n\t"  // t6 = 0xAABBGGRR or 0xRRGGBBAA

        "shll.ph         %[s1], %[t4], 11          \n\t"  // r<<11
        "shll.ph         %[t0], %[t6], 5           \n\t"  // g<<5
        "or              %[s0], %[s1], %[t0]       \n\t"
        "or              %[s1], %[s0], %[t5]       \n\t"  // 0xrgb|rgb
        "srl             %[t2], %[s1], 16          \n\t"
        "and             %[t3], %[s1], 0xFFFF      \n\t"

        "sh              %[t2], 0(%[dst])          \n\t"
        "sh              %[t3], 2(%[dst])          \n\t"
        "addiu           %[src], %[src], 8         \n\t"
        "addi            %[count], %[count], -2    \n\t"
        "b               2b                        \n\t"
        " add             %[dst], %[dst], 4        \n\t"
        "3:                                        \n\t"
        ".set reorder                              \n\t"
        : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count), [x1] "+r" (x1),
          [sc_mul] "+r" (sc_mul), [sc_add] "+r" (sc_add),
          [t0] "=&r" (t0), [t1] "+r" (t1), [t2] "=&r" (t2), [t3] "+r" (t3),
          [t4] "+r" (t4), [t5] "=&r" (t5), [t6] "=&r" (t6), [s0] "=&r" (s0),
          [s1] "=&r" (s1), [s2] "=&r" (s2), [s3] "=&r" (s3)
        : [dither] "r" (dither), [alpha] "r" (alpha)

    );

    if(count == 1) {
        SkPMColor c = *src++;
        SkPMColorAssert(c);
        SkASSERT(SkGetPackedA32(c) == 255);
        DITHER_565_SCAN(y);
        int dither = DITHER_VALUE(x);
        int sr = SkGetPackedR32(c);
        int sg = SkGetPackedG32(c);
        int sb = SkGetPackedB32(c);
        sr = SkDITHER_R32To565(sr, dither);
        sg = SkDITHER_G32To565(sg, dither);
        sb = SkDITHER_B32To565(sb, dither);

        uint16_t d = *dst;
        *dst++ = SkPackRGB16(SkAlphaBlend(sr, SkGetPackedR16(d), alpha),
                             SkAlphaBlend(sg, SkGetPackedG16(d), alpha),
                             SkAlphaBlend(sb, SkGetPackedB16(d), alpha));
        DITHER_INC_X(x);
    }
}

static void S32A_D565_Opaque_mips(uint16_t* __restrict__ dst,
                                  const SkPMColor* __restrict__ src,
                                  int count, U8CPU alpha, int x, int y) {

    asm volatile (
        "pref  0, 0(%0)   \n\t"
        "pref  1, 0(%1)   \n\t"
        "pref  0, 32(%0)  \n\t"
        "pref  1, 32(%1)  \n\t"
        ::"r"(src), "r"(dst)
    );

    register uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0, t8 = 0;
    register uint32_t t16 = 0;
    register uint32_t add_x10 = 0x100010;
    register uint32_t add_x20 = 0x200020;
    register uint32_t sa = 0xff00ff;

    __asm__ __volatile__(
        ".set noreorder                           \n\t"
        "blez           %[count], 1f              \n\t"
        " nop                                     \n\t"

        "2:                                       \n\t"
        "beqz            %[count], 1f             \n\t"
        " nop                                     \n\t"
        "addiu          %[t0], %[count], -1       \n\t"
        "beqz           %[t0], 1f                 \n\t"
        " nop                                     \n\t"
        "bnez           %[t16], 3f                \n\t"
        " nop                                     \n\t"
        "li             %[t16], 2                 \n\t"
        "pref           0, 64(%[src])             \n\t"
        "pref           1, 64(%[dst])             \n\t"

        "3:                                       \n\t"
        "addiu          %[t16], %[t16], -1        \n\t"
        "lw             %[t0], 0(%[src])          \n\t"  // t0 = sr1|sg1|sb1|sa1
        "lw             %[t1], 4(%[src])          \n\t"  // t1 = sr2|sg2|sb2|sa2
#ifdef SK_CPU_BENDIAN // BE: r|g|b|a
        "precrq.ph.w    %[t2], %[t0], %[t1]       \n\t"  // t2 = r1|g1|r2|g2
        "preceu.ph.qbla %[t3], %[t2]              \n\t"  // t3 = 0|r1|0|r2
        "preceu.ph.qbra %[t4], %[t2]              \n\t"  // t4 = 0|g1|0|g2
#ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // b1|a1|b2|a2
#else
        "sll            %[t0], %[t0], 16          \n\t"  // b1|a1|0|0
        "sll            %[t6], %[t1], 16          \n\t"  // b2|a2|0|0
        "precrq.ph.w    %[t0], %[t0], %[t6]       \n\t"  // b1|a1|b2|a2
#endif
        "preceu.ph.qbla %[t8], %[t0]              \n\t"  // t8 = 0|b1|0|b2
        "preceu.ph.qbra %[t0], %[t0]              \n\t"  // t0 = 0|a1|0|a2
#else // SK_CPU_LENDIAN LE: a|b|g|r
        "precrq.ph.w    %[t2], %[t0], %[t1]       \n\t"  // t2 = a1|b1|a2|b2
        "preceu.ph.qbra %[t8], %[t2]              \n\t"  // t8 = 0|b1|0|b2
#ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // g1|r1|g2|r2
#else
        "sll            %[t0], %[t0], 16          \n\t"  // g1|r1|0|0
        "sll            %[t6], %[t1], 16          \n\t"  // g2|r2|0|0
        "precrq.ph.w    %[t0], %[t0], %[t6]       \n\t"  // g1|r1|g2|r2
#endif
        "preceu.ph.qbra %[t3], %[t0]              \n\t"  // t3 = 0|r1|0|r2
        "preceu.ph.qbla %[t4], %[t0]              \n\t"  // t4 = 0|g1|0|g2
        "preceu.ph.qbla %[t0], %[t2]              \n\t"  // t0 = 0|a1|0|a2
#endif // SK_CPU_BENDIAN
        "subq.ph        %[t1], %[sa], %[t0]       \n\t"  // t1 = 0|255-sa1|0|255-sa2
        "sra            %[t2], %[t1], 8           \n\t"  // t1 = 0|0|255-sa1|0
        "or             %[t5], %[t2], %[t1]       \n\t"  // t5 = 0|0|255-sa1|255-sa2
        "replv.ph       %[t2], %[t5]              \n\t"  // t2 = 255-sa1|255-sa2|255-sa1|255-sa2
        "lh             %[t0], 0(%[dst])          \n\t"  // t0 = ffff||r1g1b1_565
        "lh             %[t1], 2(%[dst])          \n\t"  // t1 = ffff||r2g2b2_565
        "and            %[t1], %[t1], 0xffff      \n\t"
#ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // r1g1b1_565||r2g2b2_565
#else
        "sll            %[t5], %[t0], 16          \n\t"  // t1 = r1g1b1_565||0
        "or             %[t0], %[t5], %[t1]       \n\t"  // t0 = r1g1b1_565||r2g2b2_565
#endif
        "and            %[t1], %[t0], 0x1f001f    \n\t"  // t1 = 0|db1|0|db2
        "shra.ph        %[t6], %[t0], 11          \n\t"  // t6 = 0|dr1|0|dr2
        "and            %[t6], %[t6], 0x1f001f    \n\t"
        "and            %[t7], %[t0], 0x7e007e0   \n\t"
        "shra.ph        %[t5], %[t7], 5           \n\t"  // t5 = 0|dg1|0|dg2
        "muleu_s.ph.qbl %[t0], %[t2], %[t6]       \n\t"  // (255-sa1)*(dr1)||(255-sa2)*(dr2)
        "addq.ph        %[t7], %[t0], %[add_x10]  \n\t"
        "shra.ph        %[t6], %[t7], 5           \n\t"
        "addq.ph        %[t6], %[t7], %[t6]       \n\t"
        "shra.ph        %[t0], %[t6], 5           \n\t"
        "addq.ph        %[t7], %[t0], %[t3]       \n\t"
        "shra.ph        %[t6], %[t7], 3           \n\t"
        "muleu_s.ph.qbl %[t0], %[t2], %[t1]       \n\t"  // (255-sa1)*(db1)||(255-sa2)*(db2)
        "addq.ph        %[t7], %[t0], %[add_x10]  \n\t"
        "shra.ph        %[t0], %[t7], 5           \n\t"
        "addq.ph        %[t7], %[t7], %[t0]       \n\t"
        "shra.ph        %[t0], %[t7], 5           \n\t"
        "addq.ph        %[t7], %[t0], %[t8]       \n\t"
        "shra.ph        %[t3], %[t7], 3           \n\t"
        "muleu_s.ph.qbl %[t0], %[t2], %[t5]       \n\t"  // (255-sa1)*(dg1)||(255-sa2)*(dg2)
        "addq.ph        %[t7], %[t0], %[add_x20]  \n\t"
        "shra.ph        %[t0], %[t7], 6           \n\t"
        "addq.ph        %[t8], %[t7], %[t0]       \n\t"
        "shra.ph        %[t0], %[t8], 6           \n\t"
        "addq.ph        %[t7], %[t0], %[t4]       \n\t"
        "shra.ph        %[t8], %[t7], 2           \n\t"
        "shll.ph        %[t0], %[t8], 5           \n\t"  // g on place
        "shll.ph        %[t1], %[t6], 11          \n\t"  // r on place
        "or             %[t2], %[t0], %[t1]       \n\t"
        "or             %[t3], %[t2], %[t3]       \n\t"
        "sra            %[t4], %[t3], 16          \n\t"
        "sh             %[t4], 0(%[dst])          \n\t"
        "sh             %[t3], 2(%[dst])          \n\t"
        "addiu          %[count], %[count], -2    \n\t"
        "addiu          %[src], %[src], 8         \n\t"
        "b              2b                        \n\t"
        " addiu          %[dst], %[dst], 4        \n\t"
        "1:                                       \n\t"
        ".set reorder                             \n\t"
        : [dst] "+r" (dst), [src] "+r" (src), [count] "+r" (count), [t16] "=&r" (t16),
          [t0] "=&r" (t0), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3),
          [t4] "=&r" (t4), [t5] "=&r" (t5), [t6] "=&r" (t6), [t7] "+r" (t7), [t8] "+r" (t8)
        : [add_x10] "r" (add_x10), [add_x20] "r" (add_x20), [sa] "r" (sa)
        );
    if (count == 1)
    {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            //if (__builtin_expect(c!=0, 1))
            if (c) {
                *dst = SkSrcOver32To16(c, *dst);
            }
            dst += 1;
    }
}

static void S32A_D565_Blend_mips(uint16_t* SK_RESTRICT dst,
                                 const SkPMColor* SK_RESTRICT src, int count,
                                 U8CPU alpha, int /*x*/, int /*y*/) {
    register uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0;
    register uint32_t t8 = 0, t9 = 0;
    register uint32_t  s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    register unsigned dst_scale = 0;

    __asm__ __volatile__(
        ".set noreorder                                   \n\t"
        "replv.qb        %[t0], %[alpha]                  \n\t"  // t0 = 0|A|0|A
        "repl.ph         %[t6], 0x80                      \n\t"
        "repl.ph         %[t7], 0xFF                      \n\t"
        "1:                                               \n\t"
        "addiu           %[t8], %[count], -1              \n\t"
        "blez            %[t8], 2f                        \n\t"
        " nop                                             \n\t"
        "lw              %[t8], 0(%[src])                 \n\t"  // t8 = src1
        "lw              %[t9], 4(%[src])                 \n\t"  // t9 = src2
        "lh              %[t4], 0(%[dst])                 \n\t"  // t4 = dst1
        "lh              %[t5], 2(%[dst])                 \n\t"  // t5 = dst2
        "sll             %[t5], %[t5], 16                 \n\t"
#ifdef SK_CPU_BENDIAN
        "precrq.qb.ph    %[t1], %[t8], %[t9]              \n\t"  // 0xR1B1R2B2
        "sll             %[t2], %[t8], 8                  \n\t"
        "sll             %[t3], %[t9], 8                  \n\t"
        "precrq.qb.ph    %[t3], %[t2], %[t3]              \n\t"  // 0xG1A1G2A2
        "preceu.ph.qbra  %[t8], %[t3]                     \n\t"  // 0x00A100A2
        "muleu_s.ph.qbr  %[s3], %[t0], %[t8]              \n\t"  // a * alpha
        "preceu.ph.qbra  %[t2], %[t1]                     \n\t"  // 0x00B100B2
        "preceu.ph.qbla  %[t1], %[t1]                     \n\t"  // 0x00R100R2
        "preceu.ph.qbla  %[t3], %[t3]                     \n\t"  // 0x00G100G2
#else // SK_CPU_LENDIAN
        "sll             %[t2], %[t8], 8                  \n\t"
        "sll             %[t3], %[t9], 8                  \n\t"
        "precrq.qb.ph    %[t1], %[t2], %[t3]              \n\t"  // 0xB1R1B2R2
        "precrq.qb.ph    %[t3], %[t8], %[t9]              \n\t"  // 0xA1G1A2G2
        "preceu.ph.qbla  %[t8], %[t3]                     \n\t"  // 0x00A100A2
        "muleu_s.ph.qbr  %[s3], %[t0], %[t8]              \n\t"  // a * alpha
        "preceu.ph.qbla  %[t2], %[t1]                     \n\t"  // 0x00B100B2
        "preceu.ph.qbra  %[t1], %[t1]                     \n\t"  // 0x00R100R2
        "preceu.ph.qbra  %[t3], %[t3]                     \n\t"  // 0x00G100G2
#endif // SK_CPU_BENDIAN
        "packrl.ph       %[t9], %[t4], %[t5]              \n\t"  // 0xd1d1d2d2
        "shra.ph         %[s0], %[t9], 11                 \n\t"  // d >> 11
        "and             %[s0], %[s0], 0x1F001F           \n\t"
        "shra.ph         %[s1], %[t9], 5                  \n\t"  // g >> 5
        "and             %[s1], %[s1], 0x3F003F           \n\t"
        "and             %[s2], %[t9], 0x1F001F           \n\t"
        "addq.ph         %[s3], %[s3], %[t6]              \n\t"
        "shra.ph         %[t5], %[s3], 8                  \n\t"
        "and             %[t5], %[t5], 0xFF00FF           \n\t"
        "addq.ph         %[dst_scale], %[s3], %[t5]       \n\t"
        "shra.ph         %[dst_scale], %[dst_scale], 8    \n\t"
        "subq_s.ph       %[dst_scale], %[t7], %[dst_scale]\n\t"
        "sll             %[dst_scale], %[dst_scale], 8    \n\t"
        "precrq.qb.ph    %[dst_scale], %[dst_scale], %[dst_scale] \n\t"
        "shrl.qb         %[t1], %[t1], 3                  \n\t"  // r >> 3
        "shrl.qb         %[t2], %[t2], 3                  \n\t"  // b >> 3
        "shrl.qb         %[t3], %[t3], 2                  \n\t"  // g >> 2
        "muleu_s.ph.qbl  %[t1], %[t0], %[t1]              \n\t"  // r*alpha
        "muleu_s.ph.qbl  %[t2], %[t0], %[t2]              \n\t"  // b*alpha
        "muleu_s.ph.qbl  %[t3], %[t0], %[t3]              \n\t"  // g*alpha
        "muleu_s.ph.qbl  %[t8], %[dst_scale], %[s0]       \n\t"  // r*dst_scale
        "muleu_s.ph.qbl  %[t9], %[dst_scale], %[s2]       \n\t"  // b*dst_scale
        "muleu_s.ph.qbl  %[t4], %[dst_scale], %[s1]       \n\t"  // g*dst_scale
        "addq.ph         %[t1], %[t1], %[t8]              \n\t"
        "addq.ph         %[t2], %[t2], %[t9]              \n\t"
        "addq.ph         %[t3], %[t3], %[t4]              \n\t"
        "addq.ph         %[t8], %[t1], %[t6]              \n\t"  // dr + 128
        "addq.ph         %[t9], %[t2], %[t6]              \n\t"  // db + 128
        "addq.ph         %[t4], %[t3], %[t6]              \n\t"  // dg + 128
        "shra.ph         %[t1], %[t8], 8                  \n\t"
        "addq.ph         %[t1], %[t1], %[t8]              \n\t"
        "preceu.ph.qbla  %[t1], %[t1]                     \n\t"  // 0x00R100R2
        "shra.ph         %[t2], %[t9], 8                  \n\t"
        "addq.ph         %[t2], %[t2], %[t9]              \n\t"
        "preceu.ph.qbla  %[t2], %[t2]                     \n\t"  // 0x00B100B2
        "shra.ph         %[t3], %[t4], 8                  \n\t"
        "addq.ph         %[t3], %[t3], %[t4]              \n\t"
        "preceu.ph.qbla  %[t3], %[t3]                     \n\t"  // 0x00G100G2
        "shll.ph         %[t8], %[t1], 11                 \n\t"  // r<<11
        "shll.ph         %[t9], %[t3], 5                  \n\t"  // g<<5
        "or              %[t8], %[t8], %[t9]              \n\t"
        "or              %[s0], %[t8], %[t2]              \n\t"  // 0xrgb|rgb
        "srl             %[t8], %[s0], 16                 \n\t"
        "and             %[t9], %[s0], 0xFFFF             \n\t"
        "sh              %[t8], 0(%[dst])                 \n\t"
        "sh              %[t9], 2(%[dst])                 \n\t"
        "addiu           %[src], %[src], 8                \n\t"
        "addiu           %[count], %[count], -2           \n\t"
        "b               1b                               \n\t"
        " addiu           %[dst], %[dst], 4               \n\t"
        "2:                                               \n\t"
        ".set reorder                                     \n\t"
        : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count), [dst_scale] "+r" (dst_scale),
          [s0] "+r" (s0), [s1] "+r" (s1), [s2] "+r" (s2), [s3] "+r" (s3),
          [t0] "=&r" (t0), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "=&r" (t3), [t4] "=&r" (t4),
          [t5] "=&r" (t5), [t6] "=&r" (t6), [t7] "=&r" (t7), [t8] "=&r" (t8), [t9] "=&r" (t9)
        : [alpha] "r" (alpha)

    );

    if(count == 1) {
        SkPMColor sc = *src++;
        SkPMColorAssert(sc);
        if (sc) {
            uint16_t dc = *dst;
            unsigned dst_scale = 255 - SkMulDiv255Round(SkGetPackedA32(sc), alpha);
            unsigned dr = SkMulS16(SkPacked32ToR16(sc), alpha) + SkMulS16(SkGetPackedR16(dc), dst_scale);
            unsigned dg = SkMulS16(SkPacked32ToG16(sc), alpha) + SkMulS16(SkGetPackedG16(dc), dst_scale);
            unsigned db = SkMulS16(SkPacked32ToB16(sc), alpha) + SkMulS16(SkGetPackedB16(dc), dst_scale);
            *dst = SkPackRGB16(SkDiv255Round(dr), SkDiv255Round(dg), SkDiv255Round(db));
        }
        dst +=1;
    }
}
static void S32_Blend_BlitRow32_mips(SkPMColor* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src,
                                int count, U8CPU alpha) {
    register int32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0;

    __asm__ __volatile__ (
        ".set noreorder                          \n\t"
        "li              %[t2], 0x100            \n\t"
        "addiu           %[t0], %[alpha], 1      \n\t"
        "subu            %[t1], %[t2], %[t0]     \n\t"
        "replv.qb        %[t7], %[t0]            \n\t"  // t7 = ssc|ssc|ssc|ssc
        "replv.qb        %[t6], %[t1]            \n\t"  // t6 = dsc|dsc|dsc|dsc
        "1:                                      \n\t"
        "blez            %[count], 2f            \n\t"
        "lw              %[t0], 0(%[src])        \n\t"  // t0 = src
        "lw              %[t1], 0(%[dst])        \n\t"  // t1 = dst
        "preceu.ph.qbr   %[t2], %[t0]            \n\t"  // t2 = 0|b|0|a
        "preceu.ph.qbl   %[t3], %[t0]            \n\t"  // t3 = 0|r|0|g
        "preceu.ph.qbr   %[t4], %[t1]            \n\t"  // t4 = 0|b|0|a
        "preceu.ph.qbl   %[t5], %[t1]            \n\t"  // t5 = 0|r|0|g
        "muleu_s.ph.qbr  %[t2], %[t7], %[t2]     \n\t"
        "muleu_s.ph.qbr  %[t3], %[t7], %[t3]     \n\t"
        "muleu_s.ph.qbr  %[t4], %[t6], %[t4]     \n\t"
        "muleu_s.ph.qbr  %[t5], %[t6], %[t5]     \n\t"
        "addiu           %[src], %[src], 4       \n\t"
        "addiu           %[count], %[count], -1  \n\t"
        "precrq.qb.ph    %[t0], %[t3], %[t2]     \n\t"
        "precrq.qb.ph    %[t2], %[t5], %[t4]     \n\t"
        "addu            %[t1], %[t0], %[t2]     \n\t"
        "sw              %[t1], 0(%[dst])        \n\t"
        "b               1b                      \n\t"
        " addi           %[dst], %[dst], 4       \n\t"
        "2:                                      \n\t"
        ".set reorder                            \n\t"
        : [dst] "+r" (dst), [t0] "=&r" (t0), [t1] "=&r" (t1), [t2] "=&r" (t2), [t3] "+r" (t3),
          [t4] "+r" (t4), [t5] "+r" (t5), [t6] "=&r" (t6),  [t7] "=&r" (t7)
        : [src] "r" (src), [count] "r" (count), [alpha] "r" (alpha)
    );
}
#define S32_D565_Opaque_Dither_PROC S32_D565_Opaque_Dither_mips
#define S32A_D565_Opaque_PROC   S32A_D565_Opaque_mips
#define S32_D565_Blend_Dither_PROC  S32_D565_Blend_Dither_mips
#define S32A_D565_Blend_PROC    S32A_D565_Blend_mips
#define S32_Blend_BlitRow32_PROC  S32_Blend_BlitRow32_mips

#else // __mips_dsp

#define S32_D565_Opaque_Dither_PROC NULL
#define S32A_D565_Opaque_PROC   NULL
#define S32_D565_Blend_Dither_PROC  NULL
#define S32A_D565_Blend_PROC    NULL
#define S32_Blend_BlitRow32_PROC  NULL

#endif // __mips_dsp

static const SkBlitRow::Proc platform_565_procs[] = {
    // no dither
    NULL,
    S32_D565_Blend_PROC,
    S32A_D565_Opaque_PROC,
    S32A_D565_Blend_PROC,

    // dither
    S32_D565_Opaque_Dither_PROC,
    S32_D565_Blend_Dither_PROC,
    S32A_D565_Opaque_Dither_PROC,
    NULL,
};
static const SkBlitRow::Proc platform_4444_procs[] = {
    // no dither
    NULL,   // S32_D4444_Opaque,
    NULL,   // S32_D4444_Blend,
    NULL,   // S32A_D4444_Opaque,
    NULL,   // S32A_D4444_Blend,

    // dither
    NULL,   // S32_D4444_Opaque_Dither,
    NULL,   // S32_D4444_Blend_Dither,
    NULL,   // S32A_D4444_Opaque_Dither,
    NULL,   // S32A_D4444_Blend_Dither
};

static const SkBlitRow::Proc32 platform_32_procs[] = {
    NULL,   // S32_Opaque,
    S32_Blend_BlitRow32_PROC,   // S32_Blend,
    NULL,   // S32A_Opaque,
    NULL,   // S32A_Blend,
};

SkBlitRow::Proc SkBlitRow::PlatformProcs4444(unsigned flags) {
    return platform_4444_procs[flags];
}

SkBlitRow::Proc SkBlitRow::PlatformProcs565(unsigned flags) {
    return platform_565_procs[flags];
}

SkBlitRow::Proc32 SkBlitRow::PlatformProcs32(unsigned flags) {
    return platform_32_procs[flags];
}

SkBlitRow::ColorProc SkBlitRow::PlatformColorProc() {
    return NULL;
}

SkBlitMask::ColorProc SkBlitMask::PlatformColorProcs(SkBitmap::Config dstConfig,
                                                     SkMask::Format maskFormat,
                                                     SkColor color) {
    return NULL;
}

SkBlitMask::BlitLCD16RowProc SkBlitMask::PlatformBlitRowProcs16(bool isOpaque) {
    return NULL;
}

SkBlitMask::RowProc SkBlitMask::PlatformRowProcs(SkBitmap::Config dstConfig,
                                                 SkMask::Format maskFormat,
                                                 RowFlags flags) {
    return NULL;
}
