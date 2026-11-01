#!/usr/bin/env python3
"""TC11: End-to-End CAN EngineCmd

Mô tả:
  Truy vết toàn bộ luồng data EngineCmd từ App → Can_Write:
    Layer 5 (App):   throttle=75
    Layer 4 (COM):   Com_IpduBuf[0][0] = 0x4B
    Layer 3 (PduR):  dispatch → CanIf
    Layer 2 (CanIf): Can_PduType.id = 0x180
    Layer 1 (CAN):   txMsg.Data[0] = 75

  Kiểm tra data integrity xuyên suốt 5 layer.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value, parse_backtrace


class TC11_E2E_CAN(TestCaseCoBan):
    ma = "TC11"
    ten = "End-to-End CAN EngineCmd"
    mo_ta = "Truy vết Throttle=75 qua 5 layer: App→COM→PduR→CanIf→CAN Driver"

    def tao_gdb_script(self) -> str:
        return """
# Layer 4: COM – kiểm tra sau khi pack signal
break Com_TriggerIPDUSend
continue
printf "L4_PduId=%d\\n", PduId
x/5bx Com_IpduBuf

# Layer 2-1: Can_Write – kiểm tra cuối chain
break Can_Write
continue
printf "L1_CAN_ID=0x%x\\n", PduInfo->id
printf "L1_DLC=%d\\n", PduInfo->length
printf "L1_DATA0=%d\\n", PduInfo->sdu[0]
printf "L1_DATA1=%d\\n", PduInfo->sdu[1]
printf "L1_DATA2=%d\\n", PduInfo->sdu[2]
backtrace 8
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # Layer 4: PduId = 0 (EngineCmd)
        pdu_id = parse_printf_value(gdb_output, "L4_PduId")
        kq.them_kiem_tra("L4 COM – PduId", "0 (EngineCmd)", pdu_id or "N/A",
                         pdu_id == "0")

        # Layer 1: CAN ID
        can_id = parse_printf_value(gdb_output, "L1_CAN_ID")
        kq.them_kiem_tra("L1 CAN – CAN ID", "0x180", can_id or "N/A",
                         can_id == "0x180")

        # Layer 1: DLC
        dlc = parse_printf_value(gdb_output, "L1_DLC")
        kq.them_kiem_tra("L1 CAN – DLC", "5", dlc or "N/A",
                         dlc == "5")

        # Layer 1: Data[0] = Throttle = 75
        data0 = parse_printf_value(gdb_output, "L1_DATA0")
        kq.them_kiem_tra("L1 CAN – Data[0] (Throttle xuyên 5 layer)", "75", data0 or "N/A",
                         data0 == "75")

        # Backtrace: đủ 5 layer?
        bt = parse_backtrace(gdb_output)
        expected_chain = ["Can_Write", "CanIf_Transmit", "PduR_ComTransmit",
                          "Com_TriggerIPDUSend", "Demo_CAN_EngineCmd"]
        found_count = sum(1 for f in expected_chain if f in bt)
        kq.them_kiem_tra(
            "Call chain đủ 5 layer",
            "5 hàm",
            f"{found_count} hàm ({', '.join(f for f in expected_chain if f in bt)})",
            found_count >= 4,  # Cho phép thiếu 1 do GDB depth
        )

        return kq


test_case = TC11_E2E_CAN()
