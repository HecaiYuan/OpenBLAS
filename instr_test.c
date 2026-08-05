#include <stdio.h>
#include <lsxintrin.h>
static void show(const char *n, __m128d v){ double d[2] __attribute__((aligned(16)))={0}; __lsx_vst(v,d,0); printf("  %-10s = {%.6g, %.6g}\n", n, d[0], d[1]); }
static __m128d mk(double a, double b){ double d[2] __attribute__((aligned(16)))={a,b}; return __lsx_vld(d,0); }
int main(void){
    __m128d vs = mk(1.0, 2.0), vd;
    printf("Source vs = {1, 2}\n");
    vd = (__m128d)__lsx_vshuf4i_d((__m128i)vs, 0x00);
    printf("\n1) vshuf4i.d vs, 0x00 (expect {1,1}):\n"); show("vd", vd);
    vd = (__m128d)__lsx_vshuf4i_d((__m128i)vs, 0x01);
    printf("\n2) vshuf4i.d vs, 0x01 (expect {2,1}):\n"); show("vd", vd);
    { __m128d va = mk(10.0, 20.0);
      __m128i r = __lsx_vextrins_d((__m128i)va, (__m128i)vs, 0x10);
      printf("\n3) vextrins.d vd={10,20}, vj={1,2}, 0x10 (expect {10,1}):\n"); show("vd", (__m128d)r); }
    { __m128d va = mk(10.0, 20.0);
      __m128i r = __lsx_vextrins_d((__m128i)va, (__m128i)vs, 0x01);
      printf("\n4) vextrins.d vd={10,20}, vj={1,2}, 0x01 (expect {2,20}):\n"); show("vd", (__m128d)r); }
    { __m128d vj=mk(1,1), vk=mk(3,4), va=mk(10,20);
      __m128d r = (__m128d)__lsx_vfmadd_d((__m128i)vj, (__m128i)vk, (__m128i)va);
      printf("\n5) vfmadd.d {1,1}*{3,4}+{10,20} (expect {13,24}):\n"); show("vd", r); }
    { double a=7.5; __m128i r = __lsx_vldrepl_d(&a, 0);
      printf("\n6) vldrepl.d from 7.5 (expect {7.5,7.5}):\n"); show("vd", (__m128d)r); }
    return 0;
}
