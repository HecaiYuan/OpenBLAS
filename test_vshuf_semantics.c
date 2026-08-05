#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Test what vshuf4i.d does in LSX mode (128-bit, 2 doubles)
// We want to verify that `vshuf4i.d U0, U0, 0x00` actually broadcasts U0[0] to both lanes

static void test_vshuf4i_d(double a, double b, uint8_t imm,
                            double *out0, double *out1) {
    double in[2] = {a, b};
    double out[2] = {0, 0};

    __asm__ __volatile__(
        "vld        $vr0, %[in], 0   \n"  // vr0 = {a, b}
        "vshuf4i.d  $vr0, $vr0, %[imm] \n" // shuffle
        "vst        $vr0, %[out], 0   \n"  // store result
        : [out]"=m"(*(double(*)[2])out)
        : [in]"r"(in), [imm]"r"((uint32_t)imm)
        : "f0", "f1", "$vr0"
    );
    *out0 = out[0];
    *out1 = out[1];
}

int main() {
    double a = 1.5;
    double b = 2.5;

    printf("Testing vshuf4i.d behavior in LSX mode (128-bit, 2 doubles):\n");
    printf("Input: vr0 = {%f, %f}\n\n", a, b);

    for (uint8_t imm = 0; imm < 16; imm++) {
        double r0, r1;
        test_vshuf4i_d(a, b, imm, &r0, &r1);
        printf("imm=0x%x: result = {%f, %f}\n", imm, r0, r1);
    }

    return 0;
}
