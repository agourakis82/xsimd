/\/\/ pow zero/ { print "current_phase = \"pow-zero-section\";" }
/\/\/ pow$/ { print "current_phase = \"pow-section\";" }
/out = pow\(/ {
    print "current_phase = \"pow-operation\"; current_batch = i;"
    print
    print "current_phase = \"after-pow-operation\";"
    next
}
/_MM_SET_EXCEPTION_MASK\(mask &/ {
    print "current_phase = \"unmask-invalid\";"
    print
    print "current_phase = \"after-unmask-invalid\";"
    next
}
/CHECK_BATCH_EQ\(ref, out\)/ {
    print "current_phase = \"CHECK_BATCH_EQ\"; current_batch = i;"
    print
    print "current_phase = \"after-CHECK_BATCH_EQ\";"
    next
}
{ print }
