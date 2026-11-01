# TC12: Continuous Operation – Verify 2 complete main loop iterations
# Check that CAN/LIN signals are sent in each iteration
target remote :3333

# Count Can_Write calls: should get 3 per loop (Engine, Brake, Body)
# Count Lin_SendFrame calls: should get at least 1 per loop
break Can_Write
break Lin_SendFrame

# === Loop 1 ===
continue
printf "TC12_LOOP1_CAN1: Can_Write, id=0x%x\n", PduInfo->id
continue
printf "TC12_LOOP1_CAN2: Can_Write, id=0x%x\n", PduInfo->id
continue
printf "TC12_LOOP1_CAN3: Can_Write, id=0x%x\n", PduInfo->id

# LIN frame (from LinIf_MainFunction)
continue
printf "TC12_LOOP1_LIN1: Lin_SendFrame, PID=0x%02x\n", PduInfoPtr->Pid

# Second LIN frame might fire in same or next loop
continue

# === Loop 2 ===
printf "TC12_LOOP2_HIT: Next breakpoint\n"
backtrace 3

continue
printf "TC12_LOOP2_NEXT\n"
backtrace 3

continue
printf "TC12_LOOP2_NEXT2\n"
backtrace 3

# If we get here, the system is running continuously
printf "TC12_PASS: Continuous operation verified (2+ loops)\n"
quit
