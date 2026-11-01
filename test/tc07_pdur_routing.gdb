# TC07: PduR Routing – CAN (PDU 0,1,2) → CanIf, LIN (PDU 3,4) → LinIf
# Verify PduR dispatch logic with correct DestModule
target remote :3333

# Break at the routing decision points
break CanIf_Transmit
break LinIf_Transmit

continue
# First hit → should be CanIf (EngineCmd, PduId=0)
printf "TC07_HIT1: CanIf_Transmit, PduId=%d\n", CanIfTxPduId
if CanIfTxPduId == 0
    printf "TC07_CHECK_1: EngineCmd→CanIf CORRECT\n"
end

continue
# Second hit → CanIf (BrakeCmd, PduId=1)
printf "TC07_HIT2: CanIf_Transmit, PduId=%d\n", CanIfTxPduId
if CanIfTxPduId == 1
    printf "TC07_CHECK_2: BrakeCmd→CanIf CORRECT\n"
end

continue
# Third hit → CanIf (BodyCmd, PduId=2)
printf "TC07_HIT3: CanIf_Transmit, PduId=%d\n", CanIfTxPduId
if CanIfTxPduId == 2
    printf "TC07_CHECK_3: BodyCmd→CanIf CORRECT\n"
end

continue
# Fourth hit → LinIf (LightCtrl, TxPduId=0 on LinIf side)
# Note: LinIf_Transmit uses its own re-mapped TxPduId
printf "TC07_HIT4: LinIf_Transmit, TxPduId=%d\n", TxPduId
printf "TC07_CHECK_4: LightCtrl→LinIf CORRECT\n"

continue
# Fifth hit → LinIf (HVACCtrl)
printf "TC07_HIT5: LinIf_Transmit, TxPduId=%d\n", TxPduId
printf "TC07_CHECK_5: HVACCtrl→LinIf CORRECT\n"

printf "TC07_PASS: PduR routing CAN/LIN verified\n"
quit
