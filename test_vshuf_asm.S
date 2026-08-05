.text
.align 5

# void test_imm0(double in[2], double out[2])
.global test_imm0
.type test_imm0, @function
test_imm0:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0x00
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm0, .-test_imm0

# void test_imm_55(double in[2], double out[2])
.global test_imm_55
.type test_imm_55, @function
test_imm_55:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0x55
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_55, .-test_imm_55

# void test_imm_aa(double in[2], double out[2])
.global test_imm_aa
.type test_imm_aa, @function
test_imm_aa:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0xaa
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_aa, .-test_imm_aa

# void test_imm_ff(double in[2], double out[2])
.global test_imm_ff
.type test_imm_ff, @function
test_imm_ff:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0xff
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_ff, .-test_imm_ff

# void test_imm_01(double in[2], double out[2])
.global test_imm_01
.type test_imm_01, @function
test_imm_01:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0x01
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_01, .-test_imm_01

# void test_imm_10(double in[2], double out[2])
.global test_imm_10
.type test_imm_10, @function
test_imm_10:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0x10
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_10, .-test_imm_10

# void test_imm_11(double in[2], double out[2])
.global test_imm_11
.type test_imm_11, @function
test_imm_11:
    vld       $vr0, $r4, 0
    vshuf4i.d $vr0, $vr0, 0x11
    vst       $vr0, $r5, 0
    jirl      $r0, $r1, 0
.size test_imm_11, .-test_imm_11
