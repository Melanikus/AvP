// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the hand-written assembly versions of these maths functions, which were switched off and never
//   compiled, so code scanners can read the file; the game uses the plain C versions in mathline.cpp, as before
#ifndef INLINE_INCLUDED
#define INLINE_INCLUDED

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* 
	Standard macros.  Note that FIXED_TO_INT
	and INT_TO_FIXED are very suboptimal in 
	this version!!!
	Also, MUL_INT and ISR are ONLY intended 
	to be used in Win95 so that Saturn versions
	of the same code can be compiled using calls
	to hand optimised assembler functions, i.e.
	for code that is never intended to be run on
	a Saturn they are unnecessary.
*/

#define OUR_ABS(x)               (((x) < 0) ? -(x) : (x))
#define OUR_SIGN(x)              (((x) < 0) ? -1 : +1)
#define OUR_INT_TO_FIXED(x)      (int) ((x) * (65536))
#define OUR_FIXED_TO_INT(x)      (int) ((x) / (65536))
#define OUR_MUL_INT(a, b)        ((a) * (b))
#define OUR_ISR(a, shift)        ((a) >> (shift))

/*

 Platform Specific 64-Bit Operator Functions

 Not all compilers support 64-bit operations, and some platforms may not
 even support 64-bit numbers. Support for 64-bit operations is therefore
 provided in the platform specific fucntions below.

 For C++ a mew class could be defined. However the current system is not
 compiled as C++ and the Cygnus GNU C++ is not currently working.

*/


/* these functions are written in plain C in mathline.cpp */
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

/*
int FloatToInt(float);
#define f2i(a, b) { a = FloatToInt(b); }
*/

int SqRoot32(int A);
void FloatToInt();
extern volatile float fti_fptmp;
extern volatile int fti_itmp;

#define f2i(a, b) { \
fti_fptmp = (b); \
FloatToInt(); \
a = fti_itmp;}


int WideMul2NarrowDiv(int a, int b, int c, int d, int e);
int _Dot(VECTORCH *vptr1, VECTORCH *vptr2);
void MakeV(VECTORCH *v1, VECTORCH *v2, VECTORCH *v3);
void AddV(VECTORCH *v1, VECTORCH *v2);
void RotVect(VECTORCH *v, MATRIXCH *m);

#define RotateVector(v,m) (_RotateVector((v),(m)))
#define RotateAndCopyVector(v_in,v_out,m) (_RotateAndCopyVector((v_in),(v_out),(m)))
#define Dot(v1,v2) (_Dot((v1),(v2)))
#define DotProduct(v1,v2) (_DotProduct((v1),(v2)))

#endif
