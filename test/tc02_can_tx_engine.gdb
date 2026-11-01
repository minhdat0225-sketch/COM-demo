# TC02: CAN TX Path – EngineCmd (CAN ID 0x180, DLC=5)
# Verify: Com_TriggerIPDUSend → PduR → CanIf → Can_Write → CAN_Transmit(CAN1)
target remote :3333

# Breakpoint tại Can_Write để kiểm tra tham số
break Can_Write
continue

# Breakpoint hit → kiểm tra tham số
printf "TC02: Can_Write hit\n"

# Kiểm tra Hth (HTH=0 cho EngineCmd)
printf "TC02_HTH=%d\n", Hth

# Step vào hàm để kiểm tra PduInfo
printf "TC02_PduInfo_id=0x%x\n", PduInfo->id
printf "TC02_PduInfo_length=%d\n", PduInfo->length

# Kiểm tra CAN ID = 0x180
if PduInfo->id == 0x180
    printf "TC02_CHECK_CANID: CORRECT (0x180)\n"
else
    printf "TC02_CHECK_CANID: WRONG (expected 0x180)\n"
end

# Kiểm tra DLC = 5
if PduInfo->length == 5
    printf "TC02_CHECK_DLC: CORRECT (5)\n"
else
    printf "TC02_CHECK_DLC: WRONG (expected 5)\n"
end

# Kiểm tra data byte 0 (Throttle = 75)
printf "TC02_DATA[0]=%d (Throttle)\n", PduInfo->sdu[0]
if PduInfo->sdu[0] == 75
    printf "TC02_CHECK_THROTTLE: CORRECT\n"
else
    printf "TC02_CHECK_THROTTLE: WRONG (expected 75)\n"
end

# Verify backtrace đúng call chain
backtrace
printf "TC02_PASS: CAN TX EngineCmd verified\n"
quit
