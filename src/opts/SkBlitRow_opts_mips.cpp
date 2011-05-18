#include "SkBlitRow.h"
#include "SkColorPriv.h"
#include "SkDither.h"

#if defined ( __mips_dsp)
static void S32_D565_Blend_mips(uint16_t* SK_RESTRICT dst,
				const SkPMColor* SK_RESTRICT src, int count,
				U8CPU alpha, int /*x*/, int /*y*/) {
	register uint32_t t0, t1, t2, t3, t4, t5, t6, t7, t8;
	register uint32_t t9, s0, s1, s2, s3, s4, s5, s6, s7, v0, v1;
	alpha +=1;
	if (count >= 2) {
	  asm volatile (
		"sll %[s4], %[alpha], 8			\n\t"/*mult vector is used later*/
		"or %[s4], %[s4], %[alpha]		\n\t"
		"repl.ph %[s5], 0x1f			\n\t"
		"repl.ph %[s6], 0x3f			\n\t"
		"repl.ph %[s7], 0xff			\n\t"
		"1:					\n\t"
		"lw	%[s2], 0(%[src])		\n\t"
		"lw	%[s3], 4(%[src])		\n\t"
#if defined	(SK_CPU_LENDIAN)
		"lwr	%[s0], 0(%[dst])		\n\t"
		"lwl	%[s0], 3(%[dst])		\n\t"
		"and	%[t1], %[s0], %[s5]		\n\t"/*Get two blues from two Dst pixels*/
		"shra.ph %[t0], %[s0], 5		\n\t"/*Get two greens from two Dst pixels*/
		"and	%[t2], %[t0], %[s6]		\n\t"
  #if __mips_dspr2
		"shrl.ph %[t3], %[s0], 11		\n\t"
  #else
		"shra.ph %[t0], %[s0], 11		\n\t"/*Get two reds from two Dst pixels*/
		"and	%[t3], %[t0], %[s5]		\n\t"
  #endif /* (__mips_dspr2) */
		"precrq.ph.w %[t0], %[s3], %[s2]	\n\t"
		"shrl.qb %[t5], %[t0], 3		\n\t"
		"and	%[t4], %[t5], %[s5]		\n\t"/*t4 has two blues from src*/
		"ins	%[s2], %[s3], 16, 16		\n\t"
		"and	%[t0], %[s2], %[s7]		\n\t"
		"shrl.qb %[t6], %[t0], 3		\n\t"/*t6 has got two reds from src*/
  #if __mips_dspr2
		"shrl.ph %[t5], %[s2], 10		\n\t"
  #else
		"shra.ph %[t0], %[s2], 10		\n\t"
		"and	%[t5], %[t0], %[s6]		\n\t"/*t5 has got two greens from src*/
  #endif  /* (__mips_dspr2) */
#else  /* SK_CPU_LENDIAN */
		"lwl	%[s0], 0(%[dst])		\n\t"
		"lwr	%[s0], 3(%[dst])		\n\t"
		"and    %[t3], %[s0], %[s5]		\n\t"/*Get two reds from two Dst pixels*/
		"shra.ph %[t0], %[s0], 5                \n\t"/*Get two greens from two Dst pixels*/
		"and    %[t2], %[t0], %[s6]		\n\t"
  #ifdef __mips_dspr2
		"shrl.ph %[t1], %[s0], 11		\n\t"
  #else
		"shra.ph %[t0], %[s0], 11               \n\t"/*Get two blues from two Dst pixels*/
		"and    %[t1], %[t0], %[s5]		\n\t"
  #endif /* (__mips_dspr2) */
		"precrq.ph.w %[t0], %[s3], %[s2]	\n\t"
		"shrl.qb %[t4], %[t0], 2		\n\t"
		"and	%[t5], %[t4], %[s6]		\n\t"/*t5 has two greens from src*/
		"ins    %[s2], %[s3], 16, 16            \n\t"
		"shra.ph %[t4], %[t0], 11		\n\t"/*t6 has got two reds from src*/
		"and	%[t6], %[t4], %[s5]		\n\t"
  #ifdef __mips_dspr2
		"shrl.ph %[t4], %[s2], 11               \n\t"
  #else
		"shra.ph %[t0], %[s2], 11		\n\t"
		"and    %[t4], %[t0], %[s5]	        \n\t"/*t4 has two blues from src*/
  #endif /* (__mips_dspr2) */
#endif
		"subu.qb	%[t7], %[t4], %[t1]	\n\t"
		"subu.qb	%[t8], %[t5], %[t2]	\n\t"
		"subu.qb	%[t9], %[t6], %[t3]	\n\t"
		"muleu_s.ph.qbr	%[t4], %[s4], %[t7]	\n\t"
		"muleu_s.ph.qbr	%[t5], %[s4], %[t8]	\n\t"
		"muleu_s.ph.qbr	%[t6], %[s4], %[t9]	\n\t"
		"shra.ph	%[t7], %[t4], 8		\n\t"
		"shra.ph	%[t8], %[t5], 8		\n\t"
		"shra.ph	%[t9], %[t6], 8		\n\t"
		"addu.qb	%[t4], %[t7], %[t1]	\n\t"
		"addu.qb	%[t5], %[t8], %[t2]	\n\t"
		"addu.qb	%[t6], %[t9], %[t3]	\n\t"
		"andi	%[s0], %[t4], 0xffff		\n\t"
		"andi	%[t0], %[t5], 0xffff    	\n\t"
		"sll	%[t0], %[t0], 0x5		\n\t"
		"or	%[s0], %[s0], %[t0]		\n\t"
		"sll	%[t0], %[t6], 0xb		\n\t"
		"or	%[t0], %[t0], %[s0]		\n\t"
		"sh	%[t0], 0(%[dst])		\n\t"
		"srl	%[s1], %[t4], 16		\n\t"
		"srl    %[t0], %[t5], 16                \n\t"
		"sll    %[t5], %[t0], 5                 \n\t"
		"or	%[t0], %[t5], %[s1] 		\n\t"
		"srl	%[s0], %[t6], 16		\n\t"
		"sll	%[s2], %[s0], 0xb		\n\t"
		"or	%[s1], %[s2], %[t0]		\n\t"
		"sh	%[s1], 2(%[dst])		\n\t"
		"addiu	%[count], %[count], -2		\n\t"
		"addiu	%[src], %[src], 8		\n\t"
		"addiu	%[dst], %[dst], 4		\n\t"
		"bge	%[count], 2, 1b			\n\t"
		:[t0]"=&r"(t0), [t1]"=&r"(t1), [t2]"=&r"(t2), [t3]"=&r"(t3), [t4]"=&r"(t4),
		[t5]"=&r"(t5), [t6]"=&r"(t6), [t7]"=&r"(t7), [t8]"=&r"(t8), [t9]"=&r"(t9),
		[s0]"=&r"(s0), [s1]"=&r"(s1), [s2]"=&r"(s2), [s3]"=&r"(s3), [s4]"=&r"(s4),
		[s5]"=&r"(s5), [s6]"=&r"(s6), [s7]"=&r"(s7), [v0]"=&r"(v0), [v1]"=&r"(v1), 
		[count]"+r"(count)
		:[alpha]"r"(alpha), [dst]"r"(dst), [src]"r"(src)
		:"hi", "lo", "memory"
	  );
	}
	if (count == 1) {
		SkPMColor c = *src++;
		SkPMColorAssert(c);
		SkASSERT(SkGetPackedA32(c) == 255);
		uint16_t d = *dst;
		*dst++ = SkPackRGB16( SkAlphaBlend(SkPacked32ToR16(c),
				SkGetPackedR16(d), alpha),
				SkAlphaBlend(SkPacked32ToG16(c),
				SkGetPackedG16(d), alpha),
				SkAlphaBlend(SkPacked32ToB16(c),
				SkGetPackedB16(d),
				alpha));
	}
}

static void S32A_D565_Opaque_Dither_mips(uint16_t* __restrict__ dst,
                                      const SkPMColor* __restrict__ src,
                                      int count, U8CPU alpha, int x, int y) {
	register int32_t t0, t1, t2, t3, t4, t5, t6;
	register int32_t t7, t8, t9, t10, t11, t12, t13, t14;
	const uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
	if (count >= 2) {
	  asm volatile (
		"li     %[t12], 0x01010101		\n\t"/*257 in 16 bits for SIMD*/
		"li	%[t13], -2017			\n\t"
		"li	%[t14], 1			\n\t"
		"1:					\n\t"
		"lw	%[t1], 0(%[src])		\n\t" /*Load src1 to t1 */
		"andi	%[t3], %[x], 0x3		\n\t"
		"addiu	%[x], %[x], 1			\n\t"/*increment x*/
		"sll	%[t4], %[t3], 2			\n\t"
		"srav	%[t5], %[dither_scan], %[t4] 	\n\t"
		"andi	%[t3], %[t5], 0xf		\n\t" /*t3 has mul op1*/
		"lw	%[t2], 4(%[src])		\n\t" /*Load src2 to t2*/
		"andi   %[t4], %[x], 0x3                \n\t"
		"sll    %[t5], %[t4], 2                 \n\t"
		"srav   %[t6], %[dither_scan], %[t5]    \n\t"
		"addiu	%[x], %[x], 1			\n\t"/*increment x*/
		"ins	%[t3], %[t6], 8, 4		\n\t" /*t3 has 2 ops*/
#ifdef SK_CPU_BENDIAN
		"andi    %[t4], %[t1], 0xff             \n\t"
		"addiu	%[t0], %[t4], 1			\n\t" /*SkAlpha255To256(a)*/
		"andi    %[t4], %[t2], 0xff             \n\t"
#else
		"srl	%[t4], %[t1], 24		\n\t" /*a0*/
		"addiu	%[t0], %[t4], 1			\n\t" /*SkAlpha255To256(a)*/
		"srl	%[t4], %[t2], 24		\n\t" /*a1*/
#endif
		"addiu	%[t5], %[t4], 1			\n\t" /*SkAlpha255To256(a)*/
		"ins	%[t0], %[t5], 16, 16		\n\t" /*t0 has 2 (a+1)s*/
		"muleu_s.ph.qbr %[t4], %[t3], %[t0]	\n\t" 
		"preceu.ph.qbla %[t3], %[t4]		\n\t" /*t3 = t4>>8*/
#ifdef SK_CPU_BENDIAN
		"srl    %[t4], %[t1], 24                \n\t"
		"srl	%[t5], %[t2], 24		\n\t"
		"ins	%[t4], %[t5], 16, 16		\n\t"
#else
		"andi	%[t4], %[t1], 0xff		\n\t"/*Calculating sr from src1*/
		"ins	%[t4], %[t2], 16, 8		\n\t"/*multiple sr's in reg t4*/
#endif
		"shrl.qb %[t5], %[t4], 5		\n\t"
		"subu.qb %[t6], %[t3], %[t5]		\n\t"
		"addq.ph %[t5], %[t6], %[t4]		\n\t" /*new sr*/
#ifdef SK_CPU_BENDIAN
		"ext	%[t4], %[t1], 16, 8		\n\t"
		"srl	%[t6], %[t2], 16		\n\t"
		"ins	%[t4], %[t6], 16, 8		\n\t"
#else
		"ext	%[t4], %[t1], 8, 8		\n\t"/*Calculating sg from src1*/	
		"srl	%[t6], %[t2], 8			\n\t"
		"ins	%[t4], %[t6], 16, 8		\n\t"
#endif
		"shrl.qb %[t6], %[t4], 6		\n\t"
		"shrl.qb %[t7], %[t3], 1		\n\t"
		"subu.qb %[t8], %[t7], %[t6]		\n\t"
		"addq.ph %[t6], %[t8], %[t4]		\n\t"/*new sg*/
#ifdef SK_CPU_BENDIAN
		"ext    %[t4], %[t1], 8, 8              \n\t"
		"srl    %[t7], %[t2], 8                 \n\t"
		"ins    %[t4], %[t7], 16, 8             \n\t"
#else
		"ext	%[t4], %[t1], 16, 8		\n\t"/*Calculating sb from src1*/
		"srl	%[t7], %[t2], 16		\n\t"
		"ins	%[t4], %[t7], 16, 8		\n\t"
#endif
		"shrl.qb %[t7], %[t4], 5		\n\t"
		"subu.qb %[t8], %[t3], %[t7]            \n\t"
		"addq.ph %[t7], %[t8], %[t4]		\n\t" /*new sb*/
		"shll.ph %[t4], %[t7], 2		\n\t"/*src expanded*/
		"andi	%[t9], %[t4], 0xffff		\n\t"/*sb1*/
		"srl	%[t10], %[t4], 16		\n\t"/*sb2*/
		"andi	%[t3], %[t6], 0xffff		\n\t"/*sg1*/
		"srl	%[t4], %[t6], 16		\n\t"/*sg2*/
		"andi	%[t6], %[t5], 0xffff		\n\t"/*sr1*/
		"srl	%[t7], %[t5], 16		\n\t"/*sr2*/
		"subq.ph %[t5], %[t12], %[t0]          \n\t"
		"srl	%[t11], %[t5], 3                \n\t"/*(256-a)>>3*/
		"beqz	%[t1], 3f			\n\t"
		"lhu	%[t5], 0(%[dst])		\n\t"
		"sll	%[t1], %[t6], 13		\n\t"/*sr1 << 13*/
		"or	%[t8], %[t9], %[t1]		\n\t"/*sr1|sb1*/
		"sll	%[t1], %[t3], 24		\n\t"/*sg1 << 24*/
		"or	%[t9], %[t1], %[t8]		\n\t"/*t9 = sr1|sb1|sg1*/
		"andi	%[t3], %[t5], 0x7e0		\n\t"
		"sll	%[t6], %[t3], 0x10		\n\t"
		"and	%[t8], %[t13], %[t5]		\n\t"
		"or	%[t5], %[t6], %[t8]		\n\t"/*dst_expanded*/
		"andi	%[t6], %[t11], 0xff		\n\t"
		"mul	%[t1], %[t6], %[t5]		\n\t"/*dst_expanded*/
		"addu	%[t5], %[t1], %[t9]		\n\t"
		"srl	%[t6], %[t5], 5			\n\t"
		"and	%[t5], %[t13], %[t6]		\n\t"/*~greenmask*/
		"srl	%[t8], %[t6], 16		\n\t"
		"andi	%[t6], %[t8], 0x7e0		\n\t"
		"or	%[t1], %[t5], %[t6]		\n\t"
		"sh	%[t1], 0(%[dst])		\n\t"
		"3:					\n\t"
		"beqz	%[t2], 2f			\n\t"
		"lhu	%[t5], 2(%[dst])		\n\t"
		"sll	%[t1], %[t7], 13		\n\t"/*sr2 << 13*/
		"or	%[t8], %[t10], %[t1]		\n\t"/*sb2|sr2*/
		"sll	%[t1], %[t4], 24		\n\t"/*sg2 << 24*/
		"or	%[t9], %[t1], %[t8]		\n\t"/*t9 = sr2|srb2|sg2*/
		"andi	%[t3], %[t5], 0x7e0		\n\t"
		"sll	%[t6], %[t3], 0x10		\n\t"
		"and	%[t8], %[t13], %[t5]		\n\t"
		"or	%[t5], %[t6], %[t8]		\n\t"/*dst_expanded*/
		"srl     %[t6], %[t11], 16		\n\t"/*(256-a)>>3*/
		"mul	%[t1], %[t6], %[t5]		\n\t"/*dst_expanded*/
		"addu	%[t5], %[t1], %[t9]		\n\t"
		"srl	%[t6], %[t5], 5			\n\t"
		"and	%[t5], %[t13], %[t6]		\n\t"/*~greenmask*/
		"srl	%[t8], %[t6], 16		\n\t"
		"andi	%[t6], %[t8], 0x7e0		\n\t"
		"or	%[t1], %[t5], %[t6]		\n\t"
		"sh	%[t1], 2(%[dst])		\n\t"
		"2:					\n\t"
		"addiu	%[count], %[count], -2		\n\t"
		"addiu	%[src], %[src], 8		\n\t"
		"addiu	%[dst], %[dst], 4		\n\t"
		"bgt	%[count], %[t14], 1b		\n\t"
		: [t0]"=&r"(t0), [t1]"=&r"(t1), [src]"=&r"(src), [count]"=&r"(count), [dst]"=&r"(dst),
		  [x]"=&r"(x), [t2]"=&r"(t2), [t3]"=&r"(t3), [t4]"=&r"(t4),  [t5]"=&r"(t5),
                  [t6]"=&r"(t6), [t7]"=&r"(t7),  [t8]"=&r"(t8),  [t9]"=&r"(t9),  [t10]"=&r"(t10),  
                  [t11]"=&r"(t11), [t12]"=&r"(t12), [t13]"=&r"(t13), [t14]"=&r"(t14)
		: [dither_scan]"r"(dither_scan)
		: "hi", "lo", "memory" 
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

#define	S32A_D565_Opaque_Dither_PROC S32A_D565_Opaque_Dither_mips

#define S32_D565_Blend_PROC	S32_D565_Blend_mips
//#define S32_D565_Blend_PROC	S32A_D565_Blend_PROC
#else
#define	S32A_D565_Opaque_Dither_PROC NULL
#define S32_D565_Blend_PROC NULL
#define S32A_D565_Blend_PROC NULL
#endif

#if defined ( __mips_dsp)
static void S32_D565_Opaque_Dither_mips(uint16_t* __restrict__ dst,
                                      const SkPMColor* __restrict__ src,
                                      int count, U8CPU alpha, int x, int y) {
    uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
    register uint32_t t0, t1, t2, t3, t4, t5, t6, t7;
    register uint32_t t8, t9, t10, t11, t12, t13, t14, t15, t16;
    int dither[4];
    int i;
    for(i = 0; i < 4; i++, x++)
    {
        dither[i] = (dither_scan >> ((x&3)<<2)) & 0xF;
    }

    asm volatile (
        "ble            %[count], $0, 1f	\n\t"
        "li             %[t13], 1		\n\t"
        "li             %[t15], 0               \n\t"
        "li             %[t16], 1               \n\t"

        "2:                       		\n\t"
        "beq            %[count], %[t13], 1f	\n\t"
        "beq            %[count], $0, 1f   	\n\t"

        "beq            %[t15], %[t16], 3f	 \n\t"
        "lw             %[t0], 0(%[dither])      \n\t"
        "lw             %[t1], 4(%[dither])      \n\t"
        "li             %[t15], 1                \n\t"
        "b              4f                 	 \n\t"
        "3:                            		 \n\t"
        "lw             %[t0], 8(%[dither])   \n\t"
        "lw             %[t1], 12(%[dither])   \n\t"
        "li             %[t15], 0                \n\t"
        "4:					 \n\t"
	"sll            %[t2], %[t0], 16         \n\t"
	"or             %[t1], %[t2], %[t1]      \n\t"
        "lw             %[t0], 0(%[src])         \n\t"
        "lw             %[t2], 4(%[src])         \n\t"
#ifdef SK_CPU_BENDIAN
        // BE: r|g|b|a
        "precrq.ph.w    %[t3], %[t0], %[t2]      \n\t"      // t3 = r1|g1|r2|g2
        "preceu.ph.qbla %[t4], %[t3]             \n\t"      // t4 = 0|r1|0|r2       R
        "preceu.ph.qbra %[t5], %[t3]             \n\t"      // t5 = 0|g1|0|g2       G
  #ifdef __mips_dspr2
        "append         %[t0], %[t2], 16         \n\t"
        "preceu.ph.qbla %[t9], %[t0]             \n\t"      // t9 = 0|b1|0|b2       B
  #else
        "sll            %[t6], %[t0], 16         \n\t"      // t6 = b1|a1|0|0
        "sll            %[t7], %[t2], 16         \n\t"      // t7 = b2|a2|0|0
        "precrq.ph.w    %[t8], %[t6], %[t7]      \n\t"      // t8 = b1|a1|b2|a2
        "preceu.ph.qbla %[t9], %[t8]             \n\t"      // t9 = 0|b1|0|b2       B
  #endif
#else
        // LE: a|b|g|r
        "precrq.ph.w    %[t3], %[t0], %[t2]      \n\t"      // t3 = a1|b1|a2|b2
        "preceu.ph.qbra %[t9], %[t3]             \n\t"      // t9 = 0|b1|0|b2       B
  #ifdef __mips_dspr2
        "append         %[t0], %[t2], 16         \n\t"
        "preceu.ph.qbra %[t4], %[t0]             \n\t"      // 0|r1|0|r2
        "preceu.ph.qbla %[t5], %[t0]             \n\t"      // 0|g1|0|g2       G
  #else
        "sll            %[t6], %[t0], 16         \n\t"      // t6 = g1|r1|0|0
        "sll            %[t7], %[t2], 16         \n\t"      // t7 = g2|r2|0|0
        "precrq.ph.w    %[t8], %[t6], %[t7]      \n\t"      // t8 = g1|r1|g2|r2
        "preceu.ph.qbra %[t4], %[t8]             \n\t"      // t9 = 0|r1|0|r2       R
        "preceu.ph.qbla %[t5], %[t8]             \n\t"      // t5 = 0|g1|0|g2       G
  #endif
#endif
        "addu.qb        %[t0], %[t4], %[t1]      \n\t"
        "shra.ph        %[t2], %[t4], 5          \n\t"
        "subu.qb        %[t3], %[t0], %[t2]      \n\t"
        "shra.ph        %[t6], %[t3], 3          \n\t"      // t6 = 0|r1|0|r2 calculated R
        "addu.qb        %[t0], %[t9], %[t1]      \n\t"
        "shra.ph        %[t2], %[t9], 5          \n\t"
        "subu.qb        %[t3], %[t0], %[t2]      \n\t"
        "shra.ph        %[t7], %[t3], 3          \n\t"      // t7 = 0|b1|0|b2 calculated B

        "shra.ph        %[t0], %[t1], 1          \n\t"      // d>>1
        "shra.ph        %[t2], %[t5], 6          \n\t"      // g>>6
        "addu.qb        %[t3], %[t5], %[t0]      \n\t"
        "subu.qb        %[t4], %[t3], %[t2]      \n\t"
        "shra.ph        %[t8], %[t4], 2          \n\t"      // t8 = 0|g1|0|g2 calculated G

        "precrq.ph.w    %[t0], %[t6], %[t7]      \n\t"      // t0 = 0|r1|0|b1 calculated
  #ifdef __mips_dspr2
        "append         %[t6], %[t7], 16         \n\t"
  #else
        "sll            %[t6], %[t6], 16         \n\t"
        "sll            %[t2], %[t7], 16         \n\t"
        "precrq.ph.w    %[t6], %[t6], %[t2]      \n\t"      // 0|r2|0|b2 calculated
  #endif
        "sra            %[t4], %[t8], 16         \n\t"      // t4 = 0|0|0|g1 calculated
        "andi           %[t5], %[t8], 0xFF       \n\t"      // t5 = 0|0|0|g2 calculated

        "sll            %[t7], %[t4], 5          \n\t"      // g1 on place
        "sra            %[t8], %[t0], 5          \n\t"      // r1 on place (b1 removed from register, set to zero)
        "or             %[t9], %[t7], %[t8]      \n\t"
        "or             %[t10], %[t9], %[t0]     \n\t"
        "andi           %[t11], %[t10], 0xFFFF   \n\t"

        "sll            %[t7], %[t5], 5          \n\t"      // g2 on place
        "sra            %[t8], %[t6], 5          \n\t"      // r2 on place (b2 removed from register, set to zero)
        "or             %[t9], %[t7], %[t8]      \n\t"
        "or             %[t10], %[t9], %[t6]     \n\t"
        "and            %[t12], %[t10], 0xFFFF   \n\t"

        "sh             %[t11], 0(%[dst])        \n\t"
        "sh             %[t12], 2(%[dst])        \n\t"

        "addiu          %[count], %[count], -2   \n\t"
        "addiu          %[src], %[src], 8        \n\t"
        "addiu          %[dst], %[dst], 4        \n\t"
        "b              2b            		 \n\t"
        "1:		                         \n\t"
        :
        : [dst] "r" (dst), [src] "r" (src), [count] "r" (count), [x] "r" (x), [dither] "r" (dither),
        [t0] "r" (t0), [t1] "r" (t1), [t2] "r" (t2), [t3] "r" (t3), [t4] "r" (t4), [t5] "r" (t5), [t6] "r" (t6),
        [t7] "r" (t7), [t8] "r" (t8), [t9] "r" (t9), [t10] "r" (t10), [t11] "r" (t11), [t12] "r" (t12), [t13] "r" (t13),  [t14] "r" (t14),
        [t15] "r" (t15), [t16] "r" (t16)
	:"memory"
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
   register int32_t t0, t1, t2, t3, t4, t5, t6, t7;
   register int32_t t8, t9, t10, t11, t12, t13, t14;

   register uint16_t* dst1 = (uint16_t*)dst;
   register uint32_t* src1 = (uint32_t*)src;
   register int x1 = 0;
   register int count1 = count;
   register U8CPU alpha1 = alpha;
   register uint32_t sc_mul;
   register int diff;
   register int scale;
   register uint32_t sc_add;

#ifdef ENABLE_DITHER_MATRIX_4X4
   const uint8_t* dither_scan = gDitherMatrix_3Bit_4X4[(y) & 3];
#else
   const uint16_t dither_scan = gDitherMatrix_3Bit_16[(y) & 3];
#endif

   register int dither[4];
   int i=0;
   for(i=0; i<4;i++)
   {
      dither[i] = (dither_scan >> ((x&3)<<2)) & 0xF;
      x+=1;
   }

   __asm__ __volatile__(
            "li               %[t0], 0x100                     \n\t"
            "addi             %[alpha1], %[alpha1],1           \n\t"
            "subu             %[diff], %[t0], %[alpha1]        \n\t"
            "replv.ph         %[sc_mul], %[alpha1]             \n\t"    // sc_mul = 0|scale|0|scale
            "li               %[t14], 1                        \n\t"
            "beq              %[alpha1], $0, 1f                \n\t"
            "replv.qb         %[sc_add], %[diff]               \n\t"    // sc_add = 0|diff|0|diff
            "b                2f                      		\n\t"
            "1:                                                \n\t"
            "replv.qb         %[sc_add], %[alpha1]             \n\t"
            "2:                                       		\n\t"
            "ble              %[count1], %[t14], 3f      	\n\t"
            "lw               %[t7], 0(%[src1])                \n\t"
            "lw               %[t8], 4(%[src1])                \n\t"

            "bne              %[x1], $0, 4f        	 	\n\t"
            "lw               %[t0], 0(%[dither])               \n\t"
            "lw               %[t2], 4(%[dither])               \n\t"
            "li               %[x1], 1                          \n\t"
            "b                5f                   		\n\t"
            "4:                                    		\n\t"
            "lw               %[t0], 8(%[dither])              \n\t"
            "lw               %[t2], 12(%[dither])             \n\t"
            "li              %[x1], 0                          \n\t"
            "5:                   		               \n\t"

            "sll              %[t3], %[t0], 7                  \n\t"
            "sll              %[t4], %[t2], 7                  \n\t"

  #ifdef __mips_dspr2
            "append           %[t0], %[t2], 16                 \n\t"    // 
  #else
            "sll              %[t1], %[t0], 8                  \n\t"    // t1 = dither1 << 8;
            "sll              %[t2], %[t2], 8                  \n\t"    // t2 = dither2 << 8;
            "precrq.qb.ph     %[t0], %[t1], %[t2]              \n\t"    // t0 = 0|dither1|0|dither2
  #endif
            "precrq.qb.ph     %[t1], %[t3], %[t4]              \n\t"    // t1 = 0|dither3|0|dither4
#ifdef SK_CPU_BENDIAN
            // BE: r|g|b|a
            "precrq.qb.ph     %[t4], %[t7], %[t8]              \n\t"    // t4 = 0xR1B1R2B2
            "sll              %[t5], %[t7], 8                  \n\t"    // t5 = c1 << 8;
            "sll              %[t6], %[t8], 8                  \n\t"    // t6 = c2 << 8;
            "precrq.qb.ph     %[t6], %[t5], %[t6]              \n\t"    // t6 = 0xG1A1G2A2

            "preceu.ph.qbra   %[t5], %[t4]                     \n\t"    // t5 = 0x00B100B2
            "preceu.ph.qbla   %[t4], %[t4]                     \n\t"    // t4 = 0x00R100R2
            "preceu.ph.qbla   %[t6], %[t6]                     \n\t"    // t6 = 0x00G100G2
#else
            // LE: a|b|g|r
            "sll              %[t5], %[t7], 8                  \n\t"
            "sll              %[t6], %[t8], 8                  \n\t"
            "precrq.qb.ph     %[t4], %[t5], %[t6]              \n\t"    // t4 = 0xB1R1B2R2
            "precrq.qb.ph     %[t6], %[t7], %[t8]              \n\t"    // t6 = 0xA1G1A2G2
            "preceu.ph.qbla   %[t5], %[t4]                     \n\t"    // t5 = 0x00B100B2
            "preceu.ph.qbra   %[t4], %[t4]                     \n\t"    // t4 = 0x00R100R2
            "preceu.ph.qbra   %[t6], %[t6]                     \n\t"    // t6 = 0x00G100G2
#endif
            "lh               %[t2], 0(%[dst1])                \n\t"
            "lh               %[t10], 2(%[dst1])               \n\t"
  #ifdef __mips_dspr2
            "append           %[t2], %[t10], 16                \n\t"    // 
  #else
            "sll              %[t10], %[t10], 16               \n\t"
            "packrl.ph        %[t2], %[t2], %[t10]             \n\t"    // t2 = dst1|dst1|dst2|dst2
  #endif
            "shra.ph          %[t11], %[t2], 11                \n\t"    // r1,r2 = d1,d2>>11
            "and              %[t11], %[t11], 0x1F001F         \n\t"    // b = d & 0x1F001F
            "shra.ph          %[t12], %[t2], 5                 \n\t"    // g1,g2 = d>>5 & 0x3F
            "and              %[t12], %[t12], 0x3F003F         \n\t"    // g = g & 0x3F003F
            "and              %[t13], %[t2], 0x1F001F          \n\t"    // b = d & 0x1F001F

            "shrl.qb          %[t3], %[t4], 5                  \n\t"    // r1 >> 5 ^ r2 >> 5
            "addu.qb          %[t4], %[t4], %[t0]              \n\t"    // r + d
            "subu.qb          %[t4], %[t4], %[t3]              \n\t"    // r + d - (r>>5)
            "shrl.qb          %[t4], %[t4], 3                  \n\t"    // >>3

            "shrl.qb          %[t3], %[t5], 5                  \n\t"    // b1 >> 5 ^ b2 >> 5
            "addu.qb          %[t5], %[t5], %[t0]              \n\t"    // b + d
            "subu.qb          %[t5], %[t5], %[t3]              \n\t"    // b + d - (b>>5)
            "shrl.qb          %[t5], %[t5], 3                  \n\t"    // >>3

            "shrl.qb          %[t3], %[t6], 6                  \n\t"    // g >> 6
            "addu.qb          %[t6], %[t6], %[t1]              \n\t"
            "subu.qb          %[t6], %[t6], %[t3]              \n\t"
            "shrl.qb          %[t6], %[t6], 2                  \n\t"

            "cmpu.lt.qb       %[t4], %[t11]                    \n\t"
            "pick.qb          %[t7], %[sc_add], $0             \n\t"
            "addu.qb          %[t7], %[t7], %[t11]             \n\t"
            "subu.qb          %[t4], %[t4], %[t11]             \n\t"    // p1 - r
            "muleu_s.ph.qbl   %[t0], %[t4], %[sc_mul]          \n\t"
            "muleu_s.ph.qbr   %[t1], %[t4], %[sc_mul]          \n\t"
            "precrq.qb.ph     %[t4], %[t0], %[t1]              \n\t"
            "addu.qb          %[t4], %[t4], %[t7]              \n\t"    // t4 = 0xAABBGGRR or t4 = 0xRRGGBBAA
            "cmpu.lt.qb       %[t5], %[t13]                    \n\t"
            "pick.qb          %[t7], %[sc_add], $0             \n\t"
            "addu.qb          %[t7], %[t7], %[t13]             \n\t"
            "subu.qb          %[t5], %[t5], %[t13]             \n\t"    // p2 - b
            "muleu_s.ph.qbl   %[t0], %[t5], %[sc_mul]          \n\t"
            "muleu_s.ph.qbr   %[t1], %[t5], %[sc_mul]          \n\t"
            "precrq.qb.ph     %[t5], %[t0], %[t1]              \n\t"
            "addu.qb          %[t5], %[t5], %[t7]              \n\t"    // t5 = 0xAABBGGRR or t5 = 0xRRGGBBAA

            "cmpu.lt.qb       %[t6], %[t12]                    \n\t"
            "pick.qb          %[t7], %[sc_add], $0             \n\t"
            "addu.qb          %[t7], %[t7], %[t12]             \n\t"
            "subu.qb          %[t6], %[t6], %[t12]             \n\t"    // p3 - g
            "muleu_s.ph.qbl   %[t0], %[t6], %[sc_mul]          \n\t"
            "muleu_s.ph.qbr   %[t1], %[t6], %[sc_mul]          \n\t"
            "precrq.qb.ph     %[t6], %[t0], %[t1]              \n\t"
            "addu.qb          %[t6], %[t6], %[t7]              \n\t"    // t6 = 0xAABBGGRR or t6 = 0xRRGGBBAA

            "shll.ph          %[t8], %[t4], 11                 \n\t"    // r<<11
            "shll.ph          %[t9], %[t6], 5                  \n\t"    // g<<5
            "or               %[t7], %[t8], %[t9]              \n\t"
            "or               %[t8], %[t7], %[t5]              \n\t"    // 0xrgb|rgb
            "srl              %[t2], %[t8], 16                 \n\t"
            "and              %[t3], %[t8], 0xFFFF             \n\t"

            "sh               %[t2], 0(%[dst1])                \n\t"
            "sh               %[t3], 2(%[dst1])                \n\t"
            "addiu            %[src1], %[src1], 8              \n\t"    // src += 2
            "add              %[dst1], %[dst1], 4              \n\t"    // dst += 2
            "addi             %[count1], %[count1], -2         \n\t"    // count -= 2
            "b                2b                       		\n\t"
            "3: 	                                        \n\t"
      :
      : [t0] "r" (t0), [t1] "r" (t1), [t2] "r" (t2), [t3] "r" (t3), [t4] "r" (t4),
        [t5] "r" (t5), [t6] "r" (t6), [t7] "r" (t7), [t8] "r" (t8), [t9] "r" (t9),
        [t10] "r" (t10), [t11] "r" (t11), [t12] "r" (t12), [t13] "r" (t13), [t14] "r" (t14),
        [x1] "r" (x1), [diff] "r" (diff), [sc_add] "r" (sc_add), [alpha1] "r" (alpha1),
        [dither] "r" (dither), [sc_mul] "r" (sc_mul) , [count1] "r" (count1),
        [src1] "r" (src1), [dst1] "r" (dst1)
      );

   if(count1 == 1)
   {
        SkPMColor c = *src1++;
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

        uint16_t d = *dst1;
        *dst1++ = SkPackRGB16(SkAlphaBlend(sr, SkGetPackedR16(d), alpha1),
                                 SkAlphaBlend(sg, SkGetPackedG16(d), alpha1),
                                 SkAlphaBlend(sb, SkGetPackedB16(d), alpha1));
        DITHER_INC_X(x);
   }
}

static void S32A_D565_Opaque_mips(uint16_t* __restrict__ dst,
                                      const SkPMColor* __restrict__ src,
                                      int count, U8CPU alpha, int x, int y) {
    register uint32_t t0, t1, t2, t3, t4, t5, t6, t7, t8;
    register uint32_t t9, t10, t11, t12, t13, t14, t15;
    register uint32_t add_x10 = 0x100010;
    register uint32_t add_x20 = 0x200020;
    register uint32_t sa = 0xff00ff;
    __asm__ __volatile__(
        "ble            %[count], $0, 1f  	  \n\t"
        "li             %[t15], 1                 \n\t"

        "2:                          		  \n\t"
        "beq            %[count], %[t15], 1f	  \n\t"
        "beq            %[count], $0, 1f  	  \n\t"
        "lw             %[t0], 0(%[src])          \n\t"  // t0 = sr1|sg1|sb1|sa1
        "lw             %[t1], 4(%[src])          \n\t"  // t1 = sr2|sg2|sb2|sa2
#ifdef SK_CPU_BENDIAN
        // BE: r|g|b|a
        "precrq.ph.w    %[t2], %[t0], %[t1]       \n\t"  // t2 = r1|g1|r2|g2
        "preceu.ph.qbla %[t3], %[t2]              \n\t"  // t3 = 0|r1|0|r2       SR
        "preceu.ph.qbra %[t4], %[t2]              \n\t"  // t4 = 0|g1|0|g2       SG
  #ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // b1|a1|b2|a2
  #else
        "sll            %[t0], %[t0], 16          \n\t"  // b1|a1|0|0
        "sll            %[t6], %[t1], 16          \n\t"  // b2|a2|0|0
        "precrq.ph.w    %[t0], %[t0], %[t6]       \n\t"  // b1|a1|b2|a2
  #endif
        "preceu.ph.qbla %[t8], %[t0]              \n\t"  // t8 = 0|b1|0|b2       SB
        "preceu.ph.qbra %[t0], %[t0]              \n\t"  // t0 = 0|a1|0|a2
#else
        // LE: a|b|g|r
        "precrq.ph.w    %[t2], %[t0], %[t1]       \n\t"  // t2 = a1|b1|a2|b2
        "preceu.ph.qbra %[t8], %[t2]              \n\t"  // t8 = 0|b1|0|b2       SB
  #ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // g1|r1|g2|r2
  #else
        "sll            %[t0], %[t0], 16          \n\t"  // g1|r1|0|0
        "sll            %[t6], %[t1], 16          \n\t"  // g2|r2|0|0
        "precrq.ph.w    %[t0], %[t0], %[t6]       \n\t"  // g1|r1|g2|r2
  #endif
        "preceu.ph.qbra %[t3], %[t0]              \n\t"  // t3 = 0|r1|0|r2       SR
        "preceu.ph.qbla %[t4], %[t0]              \n\t"  // t4 = 0|g1|0|g2       SG
        "preceu.ph.qbla %[t0], %[t2]              \n\t"  // t0 = 0|a1|0|a2
#endif // SK_CPU_BENDIAN
        "subq.ph        %[t1], %[sa], %[t0]       \n\t"  // t1 = 0|255-sa1|0|255-sa2
        "sra            %[t2], %[t1], 8           \n\t"  // t1 = 0|0|255-sa1|0
        "or             %[t5], %[t2], %[t1]       \n\t"  // t5 = 0|0|255-sa1|255-sa2
        "replv.ph       %[t2], %[t5]              \n\t"  // t2 = 255-sa1|255-sa2|255-sa1|255-sa2     SA

        "lh             %[t0], 0(%[dst])          \n\t"  // t0 = ffff||r1g1b1_565
        "lh             %[t1], 2(%[dst])          \n\t"  // t1 = ffff||r2g2b2_565
        "and            %[t1], %[t1], 0xffff      \n\t"
  #ifdef __mips_dspr2
        "append         %[t0], %[t1], 16          \n\t"  // r1g1b1_565||r2g2b2_565
  #else
        "sll            %[t5], %[t0], 16          \n\t"  // t1 = r1g1b1_565||0
        "or             %[t0], %[t5], %[t1]       \n\t"  // t0 = r1g1b1_565||r2g2b2_565
  #endif
        "and            %[t1], %[t0], 0x1f001f    \n\t"  // t1 = 0|db1|0|db2 (on place and extracted)    DB
        "shra.ph        %[t6], %[t0], 11          \n\t"  // t6 = 0|dr1|0|dr2 (on place and extracted)    DR
        "and            %[t6], %[t6], 0x1f001f    \n\t"
        "and            %[t7], %[t0], 0x7e007e0   \n\t"
        "shra.ph        %[t5], %[t7], 5           \n\t"  // t5 = 0|dg1|0|dg2 (on place and extracted)    DG

        "muleu_s.ph.qbl %[t0], %[t2], %[t6]       \n\t"  // (255-sa1)*(dr1)||(255-sa2)*(dr2)
        "addq.ph        %[t7], %[t0], %[add_x10]  \n\t"
        "shra.ph        %[t9], %[t7], 5           \n\t"
        "addq.ph        %[t10], %[t7], %[t9]      \n\t"
        "shra.ph        %[t0], %[t10], 5          \n\t"
        "addq.ph        %[t7], %[t0], %[t3]       \n\t"
        "shra.ph        %[t10], %[t7], 3          \n\t"  // t10              DR_Calc

        "muleu_s.ph.qbl %[t0], %[t2], %[t1]       \n\t"  // (255-sa1)*(db1)||(255-sa2)*(db2)
        "addq.ph        %[t7], %[t0], %[add_x10]  \n\t"
        "shra.ph        %[t9], %[t7], 5           \n\t"
        "addq.ph        %[t11], %[t7], %[t9]      \n\t"
        "shra.ph        %[t0], %[t11], 5          \n\t"
        "addq.ph        %[t7], %[t0], %[t8]       \n\t"
        "shra.ph        %[t11], %[t7], 3          \n\t"  // t11              DB_Calc

        "muleu_s.ph.qbl %[t0], %[t2], %[t5]       \n\t"  // (255-sa1)*(dg1)||(255-sa2)*(dg2)
        "addq.ph        %[t7], %[t0], %[add_x20]  \n\t"
        "shra.ph        %[t9], %[t7], 6           \n\t"
        "addq.ph        %[t12], %[t7], %[t9]      \n\t"
        "shra.ph        %[t0], %[t12], 6          \n\t"
        "addq.ph        %[t7], %[t0], %[t4]       \n\t"
        "shra.ph        %[t12], %[t7], 2          \n\t"  // t12              DG_Calc

        "shll.ph        %[t0], %[t12], 5          \n\t"  // g on place
        "shll.ph        %[t1], %[t10], 11         \n\t"  // r on place
        "or             %[t2], %[t0], %[t1]       \n\t"
        "or             %[t3], %[t2], %[t11]      \n\t"
        "sra            %[t4], %[t3], 16          \n\t"

        "sh            %[t4], 0(%[dst])           \n\t"
        "sh            %[t3], 2(%[dst])           \n\t"

        "addiu          %[count], %[count], -2    \n\t"
        "addiu          %[src], %[src], 8         \n\t"
        "addiu          %[dst], %[dst], 4         \n\t"
        "b              2b            		  \n\t"
        "1: 		                          \n\t"
        :
        : [dst] "r" (dst), [src] "r" (src), [add_x10] "r" (add_x10), [count] "r" (count), [add_x20] "r" (add_x20),
        [t0] "r" (t0), [t1] "r" (t1), [t2] "r" (t2), [t3] "r" (t3), [t4] "r" (t4), [t5] "r" (t5), [t6] "r" (t6),
        [t7] "r" (t7), [t8] "r" (t8), [t9] "r" (t9), [t10] "r" (t10), [t11] "r" (t11), [t12] "r" (t12),
        [t13] "r" (t13),  [t14] "r" (t14), [t15] "r" (t15), [sa] "r" (sa)
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
   register uint32_t t0, t1, t2, t3, t4, t5, t6, t7;
   register uint32_t t8, t9, t10, t11, t12, t13, t14;
   register uint16_t* dst1 = (uint16_t*)dst;
   register uint32_t* src1 = (uint32_t*)src;
   register uint32_t  r, g, b, a;
   register uint32_t count1 = count;
   register unsigned dst_scale;

   __asm__ __volatile__(
      "replv.qb         %[t0], %[alpha]                  \n\t"    // t0 = 0|Alpha|0|Alpha
      "repl.ph          %[t6], 0x80                      \n\t"
      "repl.ph          %[t7], 0xFF                      \n\t"
      "li               %[t14], 1                        \n\t"
      "1:		                                 \n\t"
      "ble              %[count1], %[t14], 2f      	 \n\t"
      "lw               %[t8], 0(%[src1])                \n\t"    // t8 = *src
      "lw               %[t9], 4(%[src1])                \n\t"    // t9 = *(src+1)
      "lh               %[t10], 0(%[dst1])               \n\t"    // t10 = *dst
      "lh               %[t11], 2(%[dst1])               \n\t"    // t11 = *(dst+1)
      "sll              %[t11], %[t11], 16               \n\t"
#ifdef SK_CPU_BENDIAN
      "precrq.qb.ph     %[t1], %[t8], %[t9]              \n\t"    // 0xR1B1R2B2
      "sll              %[t2], %[t8], 8                  \n\t"    // t2 = *src << 8
      "sll              %[t3], %[t9], 8                  \n\t"    // t3 = *(src+1) << 8
      "precrq.qb.ph     %[t3], %[t2], %[t3]              \n\t"    // 0xG1A1G2A2
      "preceu.ph.qbra   %[t4], %[t3]                     \n\t"    // 0x00A100A2
      "muleu_s.ph.qbr   %[a], %[t0], %[t4]               \n\t"    // a * alpha

      "preceu.ph.qbra   %[t2], %[t1]                     \n\t"    // 0x00B100B2
      "preceu.ph.qbla   %[t1], %[t1]                     \n\t"    // 0x00R100R2
      "preceu.ph.qbla   %[t3], %[t3]                     \n\t"    // 0x00G100G2
#else
      "sll              %[t2], %[t8], 8                  \n\t"    // t2 = *src << 8
      "sll              %[t3], %[t9], 8                  \n\t"    // t3 = *(src+1) << 8
      "precrq.qb.ph     %[t1], %[t2], %[t3]              \n\t"    // 0xB1R1B2R2
      "precrq.qb.ph     %[t3], %[t8], %[t9]              \n\t"    // 0xA1G1A2G2
      "preceu.ph.qbla   %[t4], %[t3]                     \n\t"    // 0x00A100A2
      "muleu_s.ph.qbr   %[a], %[t0], %[t4]               \n\t"    // a * alpha
      "preceu.ph.qbla   %[t2], %[t1]                     \n\t"    // 0x00B100B2
      "preceu.ph.qbra   %[t1], %[t1]                     \n\t"    // 0x00R100R2
      "preceu.ph.qbra   %[t3], %[t3]                     \n\t"    // 0x00G100G2
#endif
      "packrl.ph        %[t12], %[t10], %[t11]           \n\t"    // 0xd1d1d2d2
      "shra.ph          %[r], %[t12], 11                 \n\t"    // d>>11
      "and              %[r], %[r], 0x1F001F             \n\t"
      "shra.ph          %[g], %[t12], 5                  \n\t"    // g>>5
      "and              %[g], %[g], 0x3F003F             \n\t"
      "and              %[b], %[t12], 0x1F001F           \n\t"

      "addq.ph          %[a], %[a], %[t6]                \n\t"
      "shra.ph          %[t13], %[a], 8                  \n\t"
      "and              %[t13], %[t13], 0xFF00FF         \n\t"
      "addq.ph          %[dst_scale], %[a], %[t13]       \n\t"
      "shra.ph          %[dst_scale], %[dst_scale], 8    \n\t"
      "subq_s.ph        %[dst_scale], %[t7], %[dst_scale]\n\t"
      "sll              %[dst_scale], %[dst_scale], 8    \n\t"
      "precrq.qb.ph     %[dst_scale], %[dst_scale], %[dst_scale] \n\t"  // 0xdsdsdsds

      "shrl.qb          %[t1], %[t1], 3                  \n\t"    // r >> 3
      "shrl.qb          %[t2], %[t2], 3                  \n\t"    // b >> 3
      "shrl.qb          %[t3], %[t3], 2                  \n\t"    // g >> 2

      "muleu_s.ph.qbl   %[t1], %[t0], %[t1]              \n\t"    // r*alpha
      "muleu_s.ph.qbl   %[t2], %[t0], %[t2]              \n\t"    // b*alpha
      "muleu_s.ph.qbl   %[t3], %[t0], %[t3]              \n\t"    // g*alpha

      "muleu_s.ph.qbl   %[t8], %[dst_scale], %[r]        \n\t"    // r*dst_scale
      "muleu_s.ph.qbl   %[t9], %[dst_scale], %[b]        \n\t"    // b*dst_scale
      "muleu_s.ph.qbl   %[t10], %[dst_scale], %[g]       \n\t"    // g*dst_scale

      "addq.ph          %[t1], %[t1], %[t8]              \n\t"
      "addq.ph          %[t2], %[t2], %[t9]              \n\t"
      "addq.ph          %[t3], %[t3], %[t10]             \n\t"

      "addq.ph          %[t8], %[t1], %[t6]              \n\t"    // dr + 128
      "addq.ph          %[t9], %[t2], %[t6]              \n\t"    // db + 128
      "addq.ph          %[t10], %[t3], %[t6]             \n\t"    // dg + 128


      "shra.ph          %[t1], %[t8], 8                  \n\t"
      "addq.ph          %[t1], %[t1], %[t8]              \n\t"
      "preceu.ph.qbla   %[t1], %[t1]                     \n\t"    // 0x00R100R2


      "shra.ph          %[t2], %[t9], 8                  \n\t"
      "addq.ph          %[t2], %[t2], %[t9]              \n\t"
      "preceu.ph.qbla   %[t2], %[t2]                     \n\t"    // 0x00B100B2

      "shra.ph          %[t3], %[t10], 8                 \n\t"
      "addq.ph          %[t3], %[t3], %[t10]             \n\t"
      "preceu.ph.qbla   %[t3], %[t3]                     \n\t"    // 0x00G100G2

      "shll.ph          %[t8], %[t1], 11                 \n\t"    // r<<11
      "shll.ph          %[t9], %[t3], 5                  \n\t"    // g<<5
      "or               %[t4], %[t8], %[t9]              \n\t"
      "or               %[t5], %[t4], %[t2]              \n\t"    // 0xrgb|rgb
      "srl              %[t8], %[t5], 16                 \n\t"
      "and              %[t9], %[t5], 0xFFFF             \n\t"

      "sh               %[t8], 0(%[dst1])                \n\t"
      "sh               %[t9], 2(%[dst1])                \n\t"
      "addiu            %[src1], %[src1], 8              \n\t"    // src += 2
      "add              %[dst1], %[dst1], 4              \n\t"    // dst += 2
      "addi             %[count1], %[count1], -2         \n\t"    // count -= 2
      "b                1b 		                  \n\t"
      "2:                               	          \n\t"
      :
      : [t0] "r" (t0), [t1] "r" (t1), [t2] "r" (t2), [t3] "r" (t3), [t4] "r" (t4),
        [t5] "r" (t5), [t6] "r" (t6), [t7] "r" (t7), [t8] "r" (t8), [t9] "r" (t9),
        [t10] "r" (t10), [t11] "r" (t11), [t12] "r" (t12), [t13] "r" (t13), [t14] "r" (t14),
        [dst_scale] "r" (dst_scale), [count1] "r" (count1), [alpha] "r" (alpha),
        [r] "r" (r), [g] "r" (g), [b] "r" (b), [a] "r" (a), [src1] "r" (src1), [dst1] "r" (dst1)
      );

   if(count1 == 1)
   {

        SkPMColor sc = *src1++;
        SkPMColorAssert(sc);
        if (sc) {
            uint16_t dc = *dst1;
            unsigned dst_scale = 255 - SkMulDiv255Round(SkGetPackedA32(sc), alpha);
            unsigned dr = SkMulS16(SkPacked32ToR16(sc), alpha) + SkMulS16(SkGetPackedR16(dc), dst_scale);
            unsigned dg = SkMulS16(SkPacked32ToG16(sc), alpha) + SkMulS16(SkGetPackedG16(dc), dst_scale);
            unsigned db = SkMulS16(SkPacked32ToB16(sc), alpha) + SkMulS16(SkGetPackedB16(dc), dst_scale);
            *dst1 = SkPackRGB16(SkDiv255Round(dr), SkDiv255Round(dg), SkDiv255Round(db));
        }
        dst1 +=1;
   }
}

static void S32_Blend_BlitRow32_mips(SkPMColor* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src,
                                int count, U8CPU alpha) {
    register int32_t t0, t1, t2, t3, t4, t5, t6, t7;
    register uint32_t* dst1 = (uint32_t*)dst;
    register uint32_t* src1 = (uint32_t*)src;
    register int count1 = count;

    __asm__ __volatile__ (
      "li               %[t2], 0x100               \n\t"
      "addi             %[t0], %[alpha], 1         \n\t"
      "subu              %[t1], %[t2], %[t0]       \n\t"
      "replv.qb         %[t7], %[t0]               \n\t"    // t7 = src_scale|src_scale|src_scale|src_scale
      "replv.qb         %[t6], %[t1]               \n\t"    // t6 = dst_scale|dst_scale|dst_scale|dst_scale
      "1:	                                   \n\t"
      "ble              %[count1], $0, 2f 	   \n\t"
      "lw               %[t0], 0(%[src1])          \n\t"    // t0 = *src
      "lw               %[t1], 0(%[dst1])          \n\t"    // t1 = *dst
      "preceu.ph.qbr    %[t2], %[t0]               \n\t"    // t2 = 0x00BB00AA
      "preceu.ph.qbl    %[t3], %[t0]               \n\t"    // t3 = 0x00RR00GG
      "muleu_s.ph.qbr   %[t4], %[t7], %[t2]        \n\t"    // src_scale * t2
      "muleu_s.ph.qbr   %[t5], %[t7], %[t3]        \n\t"    // src_scale * t3
      "precrq.qb.ph     %[t0], %[t5], %[t4]        \n\t"
      "preceu.ph.qbr    %[t2], %[t1]               \n\t"    // t2 = 0x00BB00AA
      "preceu.ph.qbl    %[t3], %[t1]               \n\t"    // t3 = 0x00RR00GG
      "muleu_s.ph.qbr   %[t4], %[t6], %[t2]        \n\t"    // dst_scale * t2
      "muleu_s.ph.qbr   %[t5], %[t6], %[t3]        \n\t"    // dst_scale * t3
      "precrq.qb.ph     %[t2], %[t5], %[t4]        \n\t"
      "addu              %[t1], %[t0], %[t2]       \n\t"    // dst = dst + t2
      "sw               %[t1], 0(%[dst1])          \n\t"
      "addi             %[src1], %[src1], 4        \n\t"    // src += 1
      "addi             %[dst1], %[dst1], 4        \n\t"    // dst += 1
      "addi             %[count1], %[count1], -1   \n\t"    // count -= 1
      "b                1b 	                   \n\t"
      "2:	                                   \n\t"
      :
      : [dst1] "r" (dst1), [src1] "r" (src1), [count1] "r" (count1), [alpha] "r" (alpha),
        [t0] "r" (t0), [t1] "r" (t1), [t2] "r" (t2), [t3] "r" (t3), [t4] "r" (t4), [t5] "r" (t5),
        [t6] "r" (t6),  [t7] "r" (t7)
    );
}

#define S32_D565_Opaque_Dither_PROC	S32_D565_Opaque_Dither_mips
#define S32A_D565_Opaque_PROC		S32A_D565_Opaque_mips
#define S32_D565_Blend_Dither_PROC	S32_D565_Blend_Dither_mips
#define S32A_D565_Blend_PROC		S32A_D565_Blend_mips
#define S32_Blend_BlitRow32_PROC	S32_Blend_BlitRow32_mips

#else	//__mips_dsp

#define S32_D565_Opaque_Dither_PROC	NULL
#define S32A_D565_Opaque_PROC		NULL
#define S32_D565_Blend_Dither_PROC	NULL
#define S32A_D565_Blend_PROC		NULL
#define S32_Blend_BlitRow32_PROC	NULL

#endif //__mips_dsp

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

SkBlitMask::Proc SkBlitMask::PlatformProcs(SkBitmap::Config dstConfig,
                                           SkColor color)
{
   return NULL;
}

