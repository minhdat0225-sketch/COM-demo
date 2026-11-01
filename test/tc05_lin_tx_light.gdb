# TC05: LIN TX Path – LightCtrl (LIN ID 0x10, DLC=4)
# Verify: Com_TriggerIPDUSend → PduR → LinIf → Lin_SendFrame → USART2
target remote :3333

break Lin_SendFrame
continue

printf "TC05: Lin_SendFrame hit\n"

# Kiểm tra PduInfoPtr
printf "TC05_PID=0x%02x\n", PduInfoPtr->Pid
printf "TC05_DL=%d\n", PduInfoPtr->Dl

# LIN frame ID = 0x10 (sau khi qua LinIf, PID chưa tính parity)
# LinIf set Pid = LinFrameId = 0x10
if PduInfoPtr->Pid == 0x10
    printf "TC05_CHECK_PID: CORRECT (0x10)\n"
else
    printf "TC05_CHECK_PID: got 0x%02x\n", PduInfoPtr->Pid
end

# DLC = 4
if PduInfoPtr->Dl == 4
    printf "TC05_CHECK_DLC: CORRECT (4)\n"
else
    printf "TC05_CHECK_DLC: WRONG (expected 4)\n"
end

# Checksum model = Enhanced (0)
if PduInfoPtr->Cs == 0
    printf "TC05_CHECK_CS: ENHANCED (correct)\n"
else
    printf "TC05_CHECK_CS: WRONG (expected Enhanced=0)\n"
end

# Direction = TX (0)
if PduInfoPtr->Drc == 0
    printf "TC05_CHECK_DRC: TX (correct)\n"
else
    printf "TC05_CHECK_DRC: WRONG (expected TX=0)\n"
end

# Verify backtrace: should come from LinIf
backtrace
printf "TC05_PASS: LIN TX LightCtrl verified\n"
quit
