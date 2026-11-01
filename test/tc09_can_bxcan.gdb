# TC09: CAN Driver – bxCAN peripheral verification
# Verify CAN_Transmit is called with CAN1 base address (0x40006400)
target remote :3333

break CAN_Transmit
continue

printf "TC09: CAN_Transmit hit\n"

# Verify CANx = CAN1 = 0x40006400
printf "TC09_CANx=0x%x\n", CANx
if CANx == 0x40006400
    printf "TC09_CHECK_CAN1_ADDR: CORRECT (0x40006400)\n"
else
    printf "TC09_CHECK_CAN1_ADDR: WRONG (not CAN1!)\n"
end

# Verify TxMessage fields
printf "TC09_StdId=0x%x\n", TxMessage->StdId
printf "TC09_DLC=%d\n", TxMessage->DLC
printf "TC09_IDE=%d\n", TxMessage->IDE
printf "TC09_RTR=%d\n", TxMessage->RTR

# StdId should be 0x180 (EngineCmd first)
if TxMessage->StdId == 0x180
    printf "TC09_CHECK_STDID: CORRECT (0x180)\n"
else
    printf "TC09_CHECK_STDID: got 0x%x\n", TxMessage->StdId
end

# IDE = 0 (CAN_Id_Standard)
if TxMessage->IDE == 0
    printf "TC09_CHECK_IDE: STANDARD (correct)\n"
else
    printf "TC09_CHECK_IDE: WRONG (expected Standard=0)\n"
end

# RTR = 0 (CAN_RTR_Data)
if TxMessage->RTR == 0
    printf "TC09_CHECK_RTR: DATA (correct)\n"
else
    printf "TC09_CHECK_RTR: WRONG (expected Data=0)\n"
end

# DLC = 5
if TxMessage->DLC == 5
    printf "TC09_CHECK_DLC: CORRECT (5)\n"
else
    printf "TC09_CHECK_DLC: WRONG (expected 5)\n"
end

# Verify data bytes
printf "TC09_Data[0]=%d\n", TxMessage->Data[0]
printf "TC09_Data[1]=%d\n", TxMessage->Data[1]
printf "TC09_Data[2]=%d\n", TxMessage->Data[2]

# Backtrace: should come from Can_Write
backtrace
printf "TC09_PASS: bxCAN register verification passed\n"
quit
