# TC06: LIN TX Path – HVACCtrl (LIN ID 0x11, DLC=3)
target remote :3333

break Lin_SendFrame
continue
# Lần 1: LightCtrl → skip
continue
# Lần 2: HVACCtrl

printf "TC06: Lin_SendFrame hit (HVACCtrl)\n"
printf "TC06_PID=0x%02x\n", PduInfoPtr->Pid
printf "TC06_DL=%d\n", PduInfoPtr->Dl

# LIN ID = 0x11
if PduInfoPtr->Pid == 0x11
    printf "TC06_CHECK_PID: CORRECT (0x11)\n"
else
    printf "TC06_CHECK_PID: WRONG (expected 0x11)\n"
end

# DLC = 3
if PduInfoPtr->Dl == 3
    printf "TC06_CHECK_DLC: CORRECT (3)\n"
else
    printf "TC06_CHECK_DLC: WRONG (expected 3)\n"
end

# Data byte 0 = FanSpeed = 128
printf "TC06_DATA[0]=%d (FanSpeed)\n", PduInfoPtr->SduPtr[0]
if PduInfoPtr->SduPtr[0] == 128
    printf "TC06_CHECK_FANSPEED: CORRECT (128)\n"
else
    printf "TC06_CHECK_FANSPEED: WRONG (expected 128)\n"
end

backtrace
printf "TC06_PASS: LIN TX HVACCtrl verified\n"
quit
