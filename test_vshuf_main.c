#include <stdio.h>

extern void test_imm0(double in[2], double out[2]);
extern void test_imm_55(double in[2], double out[2]);
extern void test_imm_aa(double in[2], double out[2]);
extern void test_imm_ff(double in[2], double out[2]);
extern void test_imm_01(double in[2], double out[2]);
extern void test_imm_10(double in[2], double out[2]);
extern void test_imm_11(double in[2], double out[2]);

typedef void (*test_fn)(double in[2], double out[2]);

int main() {
    double in[2] = {1.5, 2.5};
    double out[2] = {0, 0};

    printf("Testing vshuf4i.d behavior in LSX mode (128-bit, 2 doubles):\n");
    printf("Input: vr = {%f, %f}\n\n", in[0], in[1]);

    struct { const char *name; test_fn fn; } tests[] = {
        {"0x00 (used in dgemv kernel for broadcast)", test_imm0},
        {"0x55", test_imm_55},
        {"0xaa", test_imm_aa},
        {"0xff", test_imm_ff},
        {"0x01", test_imm_01},
        {"0x10", test_imm_10},
        {"0x11", test_imm_11},
    };

    for (int i = 0; i < (int)(sizeof(tests)/sizeof(tests[0])); i++) {
        out[0] = 0; out[1] = 0;
        tests[i].fn(in, out);
        printf("imm=%-20s result = {%f, %f}\n", tests[i].name, out[0], out[1]);
    }

    return 0;
}
