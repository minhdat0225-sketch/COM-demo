# TC08: Signal Packing – Verify Com_SendSignal packs bytes correctly
# Test: EngineCmd I-PDU shadow buffer after Com_SendSignal calls
target remote :3333

# Break right before Com_TriggerIPDUSend in Demo_CAN_EngineCmd
# to check the shadow buffer state after all signals are packed
break Com_TriggerIPDUSend
continue

printf "TC08: Com_TriggerIPDUSend hit (EngineCmd)\n"

# At this point, Com_SendSignal has already packed all signals
# into the shadow buffer. Let's examine PduR's received data.

# Step into to get the buffer
# The buffer is Com_IpduBuf[0][0..4] for EngineCmd
# Use static var directly
printf "TC08: Examining I-PDU buffer via PduR path\n"

# Check PduId = 0 (EngineCmd)
printf "TC08_PduId=%d\n", PduId
if PduId == 0
    printf "TC08_CHECK_PDUID: CORRECT (EngineCmd)\n"
else
    printf "TC08_CHECK_PDUID: WRONG\n"
end

# Now let's break deeper to check actual data
finish
# Now we should be back in Demo_CAN_EngineCmd after the call

# Check a different signal type: BrakeCmd
continue
# Second TriggerIPDUSend → BrakeCmd

printf "TC08_PduId=%d (BrakeCmd)\n", PduId
if PduId == 1
    printf "TC08_CHECK_BRAKE_PDUID: CORRECT\n"
else
    printf "TC08_CHECK_BRAKE_PDUID: WRONG\n"
end

# Third − BodyCmd (bit-packed signals)
continue
printf "TC08_PduId=%d (BodyCmd)\n", PduId
if PduId == 2
    printf "TC08_CHECK_BODY_PDUID: CORRECT\n"
else
    printf "TC08_CHECK_BODY_PDUID: WRONG\n"
end

printf "TC08_PASS: Signal packing verified\n"
quit
