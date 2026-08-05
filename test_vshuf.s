.text

.global test
test:
    # Test 1: vshuf4i.d in LSX context - what does this encode to?
    vshuf4i.d  $vr1, $vr2, 0x00

    # Test 4: vldrepl.d (load and replicate)
    vldrepl.d   $vr1, $r3, 0x00

    # Test 5: xvshuf4i.d (LASX d-shuffle, for comparison)
    xvshuf4i.d  $xr1, $xr2, 0x00

    .end
