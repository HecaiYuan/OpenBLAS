/* Minimal config.h for standalone DGEMV LSX kernel compilation */
#ifndef CONFIG_H
#define CONFIG_H

#define OPENBLAS_ARCH_LOONGARCH64
#define OPENBLAS_OS_LINUX
#define __64BIT__
#define USE64BITINT

#ifndef ASSEMBLER
typedef long BLASLONG;
typedef unsigned long BLASULONG;
typedef int blasint;
typedef double xdouble;
typedef unsigned short bfloat16;
typedef unsigned short hfloat16;

#define OPENBLAS_COMPLEX_C99
typedef float _Complex openblas_complex_float;
typedef double _Complex openblas_complex_double;
typedef xdouble _Complex openblas_complex_xdouble;
#define openblas_make_complex_float(r,i)  ((r)+((i)*_Complex_I))
#define openblas_make_complex_double(r,i) ((r)+((i)*_Complex_I))
#define openblas_make_complex_xdouble(r,i) ((r)+((i)*_Complex_I))
#define openblas_complex_float_real(z)  (__real__ z)
#define openblas_complex_float_imag(z)  (__imag__ z)
#define openblas_complex_double_real(z) (__real__ z)
#define openblas_complex_double_imag(z) (__imag__ z)
#define openblas_complex_xdouble_real(z) (__real__ z)
#define openblas_complex_xdouble_imag(z) (__imag__ z)
#endif

#define BASE_SHIFT  3   /* DOUBLE: 2^3 = 8 bytes */

#endif /* CONFIG_H */
