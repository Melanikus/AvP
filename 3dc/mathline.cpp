// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the hand-written assembly versions of these maths functions, which were never compiled; the C
//   versions the game uses are unchanged
#include <math.h>

#include "3dc.h"

void ADD_LL(LONGLONGCH *a, LONGLONGCH *b, LONGLONGCH *c);
void ADD_LL_PP(LONGLONGCH *c, LONGLONGCH *a);
void SUB_LL(LONGLONGCH *a, LONGLONGCH *b, LONGLONGCH *c);
void SUB_LL_MM(LONGLONGCH *c, LONGLONGCH *a);
void MUL_I_WIDE(int a, int b, LONGLONGCH *c);
int CMP_LL(LONGLONGCH *a, LONGLONGCH *b);
void EQUALS_LL(LONGLONGCH *a, LONGLONGCH *b);
void NEG_LL(LONGLONGCH *a);
void ASR_LL(LONGLONGCH *a, int shift);
void IntToLL(LONGLONGCH *a, int *b);
int MUL_FIXED(int a, int b);
int DIV_FIXED(int a, int b);

int NarrowDivide(LONGLONGCH *a, int b);
int WideMulNarrowDiv(int a, int b, int c);
void RotateVector_ASM(VECTORCH *v, MATRIXCH *m);
void RotateAndCopyVector_ASM(VECTORCH *v1, VECTORCH *v2, MATRIXCH *m);

// This file used to hold hand-written x86 assembly versions of these functions as well (switched by ASM386 and
// USE_ASM). They were never compiled in this port: the plain C versions below are the ones the game has always
// used. The assembly was deleted because code scanners cannot read it.

static __int64 ConvertToLongLong(const LONGLONGCH* llch)
{
	__int64 ll;
	
	ll = ((__int64)llch->hi32 << 32) | ((__int64)llch->lo32 << 0);
	
	return ll;
}

static void ConvertFromLongLong(LONGLONGCH* llch, const __int64* ll)
{
	llch->lo32 = (unsigned int)((*ll>> 0) & 0xffffffff);
	llch->hi32 = (  signed int)((*ll>>32) & 0xffffffff);	
}

void ADD_LL(LONGLONGCH *a, LONGLONGCH *b, LONGLONGCH *c)
{
/*
	_asm
	{
		mov esi,a
		mov edi,b
		mov ebx,c
		mov	eax,[esi]
		mov	edx,[esi+4]
		add	eax,[edi]
		adc	edx,[edi+4]
		mov	[ebx],eax
		mov	[ebx+4],edx
	}
*/
	__int64 aa = ConvertToLongLong(a);
	__int64 bb = ConvertToLongLong(b);
	
	__int64 cc = aa + bb;
	
	ConvertFromLongLong(c, &cc);

}

/* ADD ++ */

void ADD_LL_PP(LONGLONGCH *c, LONGLONGCH *a)
{
/*
	_asm
	{
		mov edi,c
		mov esi,a
		mov	eax,[esi]
	 	mov	edx,[esi+4]
		add	[edi],eax
		adc	[edi+4],edx
	}
*/

	__int64 cc = ConvertToLongLong(c);
	__int64 aa = ConvertToLongLong(a);
	
	cc += aa;
	
	ConvertFromLongLong(c, &cc);
}

/* SUB */

void SUB_LL(LONGLONGCH *a, LONGLONGCH *b, LONGLONGCH *c)
{
/*
	_asm
	{
		mov esi,a
		mov edi,b
		mov ebx,c
		mov	eax,[esi]
		mov	edx,[esi+4]
		sub	eax,[edi]
		sbb	edx,[edi+4]
		mov	[ebx],eax
		mov	[ebx+4],edx
	}
*/
	__int64 aa = ConvertToLongLong(a);
	__int64 bb = ConvertToLongLong(b);
	
	__int64 cc = aa - bb;
	
	ConvertFromLongLong(c, &cc);
}

/* SUB -- */

void SUB_LL_MM(LONGLONGCH *c, LONGLONGCH *a)
{
/*
	_asm
	{
		mov edi,c
		mov esi,a
		mov	eax,[esi]
		mov	edx,[esi+4]
		sub	[edi],eax
		sbb	[edi+4],edx
	}
*/
	__int64 cc = ConvertToLongLong(c);
	__int64 aa = ConvertToLongLong(a);
	
	cc -= aa;
	
	ConvertFromLongLong(c, &cc);
}

/*

 MUL

 This is the multiply we use, the 32 x 32 = 64 widening version

*/

void MUL_I_WIDE(int a, int b, LONGLONGCH *c)
{
/*
	_asm
	{
		mov eax,a
		mov ebx,c
		imul b
		mov	[ebx],eax
		mov	[ebx+4],edx
	}
*/
	__int64 aa = (__int64) a;
	__int64 bb = (__int64) b;
	
	__int64 cc = aa * bb;
	
	ConvertFromLongLong(c, &cc);
}

/*

 CMP

 This substitutes for ==, >, <, >=, <=

*/

int CMP_LL(LONGLONGCH *a, LONGLONGCH *b)
{
/*
	int retval;
	_asm
	{
		mov ebx,a
		mov ecx,b
		mov	eax,[ebx]
		mov	edx,[ebx+4]
		sub	eax,[ecx]
		sbb	edx,[ecx+4]
		and	edx,edx
		jne	llnz
		and	eax,eax
		je	llgs
		llnz:
		mov	retval,1
		and	edx,edx
		jge	llgs
		neg	retval
		llgs:
	}
*/
	if (a->hi32 > b->hi32)
		return 1;
	else if (a->hi32 < b->hi32)
		return -1;
	else if (a->lo32 > b->lo32)
		return 1;
	else if (a->lo32 < b->lo32)
		return -1;
	else
		return 0;
}

/* EQUALS */

void EQUALS_LL(LONGLONGCH *a, LONGLONGCH *b)
{
/*
	_asm
	{
		mov edi,a
		mov esi,b
		mov	eax,[esi]
		mov	edx,[esi+4]
		mov	[edi],eax
		mov	[edi+4],edx
	}
*/
	*a = *b;
}

/* NEGATE */

void NEG_LL(LONGLONGCH *a)
{
/*
	_asm
	{
		mov esi,a
		not	dword ptr[esi]
		not	dword ptr[esi+4]
		add	dword ptr[esi],1
		adc	dword ptr[esi+4],0
	}
*/
	__int64 aa = ConvertToLongLong(a);
	
	aa = -aa;
	
	ConvertFromLongLong(a, &aa);
}

/* ASR */

void ASR_LL(LONGLONGCH *a, int shift)
{
/*
	_asm
	{
		mov esi,a
		mov eax,shift
		and	eax,eax
		jle	asrdn
		asrlp:
		sar	dword ptr[esi+4],1
		rcr	dword ptr[esi],1
		dec	eax
		jne	asrlp
		asrdn:
	}
*/
	__int64 aa = ConvertToLongLong(a);
	
	aa >>= shift;
	
	ConvertFromLongLong(a, &aa);
}

/* Convert int to LONGLONGCH */

void IntToLL(LONGLONGCH *a, int *b)
{
/*
	_asm
	{
		mov esi,b
		mov edi,a
		mov	eax,[esi]
		cdq
		mov	[edi],eax
		mov	[edi+4],edx
	}
*/
	__int64 aa = (__int64) *b;
	
	ConvertFromLongLong(a, &aa);
}

/*

 Fixed Point Multiply.


 16.16 * 16.16 -> 16.16
 or
 16.16 * 0.32 -> 0.32

 A proper version of this function ought to read
 16.16 * 16.16 -> 32.16
 but this would require a __int64 result

 Algorithm:

 Take the mid 32 bits of the 64 bit result

*/

// shift a to the right by 16, return the lower 32 bits:

fixed_t MUL_FIXED(fixed_t a, fixed_t b)
{

	return (((__int64)a * b) >> ONE_FIXED_SHIFT) & 0xFFFFFFFF;

}

/**
* Fixed point division
* \param a Fixed-point multiplicand
* \param b Fixed-point multiplier
* \returns integral result of division
*/
fixed_t DIV_FIXED(fixed_t a, fixed_t b)
{
	if (b == 0)
	{
		printf("DEBUG THIS: a = %d, b = %d\n", a, b);
		return 0; /* TODO: debug this! (start with alien on ferarco) */
	}


	__int64 aa = (__int64) a;
	__int64 bb = (__int64) b;
	__int64 cc = (aa << ONE_FIXED_SHIFT) / bb;

	return (fixed_t) (cc & 0xffffffff);

}

/*

 Multiply and Divide Functions.

*/


/*

/*

 A Narrowing 64/32 Division

*/

int NarrowDivide(LONGLONGCH *a, int b)
{
/*
	int retval;
	_asm
	{
		mov esi,a
		mov	eax,[esi]
		mov	edx,[esi+4]
		idiv	b
		mov retval,eax
	}
*/
	__int64 aa = ConvertToLongLong(a);
	__int64 bb = (__int64) b;
	
	__int64 cc = aa / bb;
	
	return (int) (cc & 0xffffffff);
}

/*

 This function performs a Widening Multiply followed by a Narrowing Divide.

 a = (a * b) / c

*/

int WideMulNarrowDiv(int a, int b, int c)
{
/*
	int retval;
	_asm
	{
		mov eax,a
		imul b
		idiv c
		mov retval,eax
	}
*/
	__int64 aa = (__int64) a;
	__int64 bb = (__int64) b;
	__int64 cc = (__int64) c;
	
	__int64 dd = (aa * bb) / cc;
	
	return (int) (dd & 0xffffffff);
}

/*

 Square Root

 Returns the Square Root of a 32-bit number

*/

int SqRoot32(int A)
{
/*
	_asm
	{
		finit
		fild A
		fsqrt
		fistp temp2
		fwait
	}
*/

	return (int) sqrt( (float)A );
}

/*

 This may look ugly (it is) but it is a MUCH faster way to convert "float" into "int" than
 the function call "CHP" used by the WATCOM compiler.

*/

volatile float fti_fptmp;
volatile int fti_itmp;

//#include <intrin.h>

void FloatToInt()
{
	fti_itmp = (int)fti_fptmp;
/*
__asm
{
	fld		fti_fptmp
	fistp	fti_itmp
}
*/
// intrinsic version - 	fti_itmp = _mm_cvt_ss2si( _mm_load_ss(&fti_fptmp) );
}
