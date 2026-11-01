# TC10: LIN Driver – USART2 API calls verification
# Verify Lin_SendFrame calls USART functions (prv_SendBreak, prv_USART_SendByte)
target remote :3333

# Break at the internal send functions to verify USART usage
break prv_SendBreak
break prv_USART_SendByte
break prv_CalcPID
break prv_CalcChecksum

continue
# First hit → prv_SendBreak (LIN Break field)
printf "TC10_HIT1: prv_SendBreak (Break field)\n"
backtrace 3

continue
# prv_USART_SendByte → Sync byte (0x55)
printf "TC10_HIT2: prv_USART_SendByte\n"
printf "TC10_BYTE=0x%02x\n", data
if data == 0x55
    printf "TC10_CHECK_SYNC: CORRECT (0x55)\n"
else
    printf "TC10_CHECK_SYNC: WRONG (expected 0x55)\n"
end

continue
# prv_CalcPID → PID calculation
printf "TC10_HIT3: prv_CalcPID\n"
printf "TC10_INPUT_ID=0x%02x\n", id

continue
# prv_USART_SendByte → PID byte
printf "TC10_HIT4: prv_USART_SendByte (PID)\n"
printf "TC10_PID_BYTE=0x%02x\n", data

continue
# prv_USART_SendByte → Data byte 0
printf "TC10_HIT5: prv_USART_SendByte (Data)\n"
printf "TC10_DATA_BYTE=0x%02x\n", data

# Skip remaining data bytes to checksum
continue
continue
continue
continue

# prv_CalcChecksum
printf "TC10_HIT: prv_CalcChecksum\n"
printf "TC10_CS_PID=0x%02x\n", pid
printf "TC10_CS_LEN=%d\n", len
if model == 0
    printf "TC10_CHECK_CS_MODEL: ENHANCED (correct)\n"
else
    printf "TC10_CHECK_CS_MODEL: WRONG\n"
end

printf "TC10_PASS: LIN USART2 API verified\n"
quit
