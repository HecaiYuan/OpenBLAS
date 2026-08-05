#include <stdio.h>
#include <stdint.h>
extern void test_instr2(const double *in, double *out);
int main(void) {
    double in[2] __attribute__((aligned(16))) = {1.0, 2.0};
    double out[12] __attribute__((aligned(16))) = {0};
    test_instr2(in, out);
    const char *names[6] = {"A: dest!=src,imm0", "B: dest==src,imm0",
                            "C: dest!=src,imm1", "D: dest==src,imm1",
                            "F: vldrepl in[0]", "G: dest==src(imm0) after fld"};
    double exp[6][2] = {
        {1,1}, {1,1},   /* imm0 broadcast elem0 */
        {2,1}, {2,1},   /* imm1: {elem1, elem0} = {2,1} */
        {1,1},          /* vldrepl of in[0]=1.0 */
        {1,1}           /* should still broadcast elem0 */
    };
    int bad = 0;
    for (int i = 0; i < 6; i++) {
        double a = out[2*i], b = out[2*i+1];
        uint64_t *p = (uint64_t*)&out[2*i];
        int ok = (a == exp[i][0] && b == exp[i][1]);
        printf("  [%s] got={%.6g, %.6g} (hex %016lx %016lx) expect={%.6g, %.6g}  %s\n",
               names[i], a, b, p[0], p[1], exp[i][0], exp[i][1],
               ok ? "OK" : "*** WRONG ***");
        if (!ok) bad = 1;
    }
    printf("\n%s\n", bad ? "FAIL" : "PASS");
    return bad;
}
