    .text
    .align 5
    .global test_instr2
    .type   test_instr2, @function
# a0=$r4: in[]  (vs={1,2} at +0)
# a1=$r5: out[] (8 vectors = 16 doubles)
test_instr2:
    vld     $vr0, $r4, 0          # v0 = {1,2}

    # Case A: dest != src  (my original failing case)
    vshuf4i.d $vr1, $vr0, 0x00
    vst     $vr1, $r5, 0          # out[0..1]

    # Case B: dest == src  (kernel's pattern)
    vld     $vr2, $r4, 0          # v2 = {1,2}
    vshuf4i.d $vr2, $vr2, 0x00
    vst     $vr2, $r5, 16         # out[2..3]

    # Case C: dest != src, imm=0x01
    vshuf4i.d $vr3, $vr0, 0x01
    vst     $vr3, $r5, 32         # out[4..5]

    # Case D: dest == src, imm=0x01
    vld     $vr4, $r4, 0
    vshuf4i.d $vr4, $vr4, 0x01
    vst     $vr4, $r5, 48         # out[6..7]

    # Case E: use vpermi.d if it exists? try vshuf.d (3-operand) instead
    # vshuf.d vd, vj, vk  (no immediate) -- not what kernel uses, skip

    # Case F: broadcast via vreplve0.d (element 0 replicate) for comparison
    vldrepl.d $vr5, $r4, 0        # replicate the double at in[0] (=1.0)
    vst     $vr5, $r5, 64         # out[8..9]

    # Case G: what does vshuf4i.d produce if we init dest first?
    vld     $vr6, $r4, 0          # v6 = {1,2}
    fld.d   $f7, $r4, 0           # f7 = 1.0  (low of v7 = 1.0; high stale)
    vshuf4i.d $vr6, $vr6, 0x00
    vst     $vr6, $r5, 80         # out[10..11]

    # Case H: vshuf4i.w on a float source for comparison
    # (skip - different width)

    jirl    $r0, $r1, 0
    .end    test_instr2
