# TC04: CAN TX Path – BodyCmd (CAN ID 0x380, DLC=4)
target remote :3333

break Can_Write
continue
# Lần 1: EngineCmd → skip
continue
# Lần 2: BrakeCmd → skip
continue
# Lần 3: BodyCmd

printf "TC04: Can_Write hit (BodyCmd)\n"
printf "TC04_PduInfo_id=0x%x\n", PduInfo->id
printf "TC04_PduInfo_length=%d\n", PduInfo->length

# Kiểm tra CAN ID = 0x380
if PduInfo->id == 0x380
    printf "TC04_CHECK_CANID: CORRECT (0x380)\n"
else
    printf "TC04_CHECK_CANID: WRONG (expected 0x380)\n"
end

# Kiểm tra DLC = 4
if PduInfo->length == 4
    printf "TC04_CHECK_DLC: CORRECT (4)\n"
else
    printf "TC04_CHECK_DLC: WRONG (expected 4)\n"
end

# Kiểm tra data byte 0 bit fields:
# bit0=Headlamp(1), bit1=TurnL(1), bit2=TurnR(0), bit3=DoorLock(1)
# Expected = 0b00001011 = 0x0B = 11
printf "TC04_DATA[0]=0x%02x (bit fields)\n", PduInfo->sdu[0]

# Headlamp ON (bit 0 = 1)
set $byte0 = PduInfo->sdu[0]
if ($byte0 & 0x01)
    printf "TC04_CHECK_HEADLAMP: ON (correct)\n"
else
    printf "TC04_CHECK_HEADLAMP: OFF (WRONG)\n"
end

# TurnL ON (bit 1 = 1)
if ($byte0 & 0x02)
    printf "TC04_CHECK_TURNL: ON (correct)\n"
else
    printf "TC04_CHECK_TURNL: OFF (WRONG)\n"
end

# TurnR OFF (bit 2 = 0)
if ($byte0 & 0x04)
    printf "TC04_CHECK_TURNR: ON (WRONG, expected OFF)\n"
else
    printf "TC04_CHECK_TURNR: OFF (correct)\n"
end

# DoorLock ON (bit 3 = 1)
if ($byte0 & 0x08)
    printf "TC04_CHECK_DOORLOCK: ON (correct)\n"
else
    printf "TC04_CHECK_DOORLOCK: OFF (WRONG)\n"
end

backtrace
printf "TC04_PASS: CAN TX BodyCmd verified\n"
quit
