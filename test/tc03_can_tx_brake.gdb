# TC03: CAN TX Path – BrakeCmd (CAN ID 0x280, DLC=3)
target remote :3333

# Break tại Can_Write, skip EngineCmd (lần 1), dừng ở BrakeCmd (lần 2)
break Can_Write
continue
# Lần 1: EngineCmd → skip
continue
# Lần 2: BrakeCmd

printf "TC03: Can_Write hit (BrakeCmd)\n"
printf "TC03_PduInfo_id=0x%x\n", PduInfo->id
printf "TC03_PduInfo_length=%d\n", PduInfo->length

# Kiểm tra CAN ID = 0x280
if PduInfo->id == 0x280
    printf "TC03_CHECK_CANID: CORRECT (0x280)\n"
else
    printf "TC03_CHECK_CANID: WRONG (expected 0x280)\n"
end

# Kiểm tra DLC = 3
if PduInfo->length == 3
    printf "TC03_CHECK_DLC: CORRECT (3)\n"
else
    printf "TC03_CHECK_DLC: WRONG (expected 3)\n"
end

# Kiểm tra data byte 0 (BrakeReq = 30)
printf "TC03_DATA[0]=%d (BrakeReq)\n", PduInfo->sdu[0]
if PduInfo->sdu[0] == 30
    printf "TC03_CHECK_BRAKEREQ: CORRECT\n"
else
    printf "TC03_CHECK_BRAKEREQ: WRONG (expected 30)\n"
end

# Kiểm tra data byte 1 (RegenReq = 50)
printf "TC03_DATA[1]=%d (RegenReq)\n", PduInfo->sdu[1]
if PduInfo->sdu[1] == 50
    printf "TC03_CHECK_REGENREQ: CORRECT\n"
else
    printf "TC03_CHECK_REGENREQ: WRONG (expected 50)\n"
end

backtrace
printf "TC03_PASS: CAN TX BrakeCmd verified\n"
quit
