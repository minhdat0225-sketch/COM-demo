#!/usr/bin/env python3
"""TC12: End-to-End LIN LightCtrl

Mô tả:
  Truy vết luồng data LightCtrl từ App → Lin_SendFrame:
    Layer 5 (App):   brightness=80
    Layer 4 (COM):   Com_IpduBuf[3][1] = 0x50
    Layer 3 (PduR):  dispatch → LinIf
    Layer 2 (LinIf): s_txBuf → Lin_SendFrame
    Layer 1 (LIN):   USART2 TX: Break+Sync+PID+Data+Checksum
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value, parse_backtrace


class TC12_E2E_LIN(TestCaseCoBan):
    ma = "TC12"
    ten = "End-to-End LIN LightCtrl"
    mo_ta = "Truy vết Brightness=80 qua 5 layer: App→COM→PduR→LinIf→LIN Driver"

    def tao_gdb_script(self) -> str:
        return """
break Lin_SendFrame
continue
printf "L1_LIN_PID=0x%02x\\n", PduInfoPtr->Pid
printf "L1_LIN_DL=%d\\n", PduInfoPtr->Dl
printf "L1_LIN_CS=%d\\n", PduInfoPtr->Cs
printf "L1_LIN_DATA0=0x%02x\\n", PduInfoPtr->SduPtr[0]
printf "L1_LIN_DATA1=0x%02x\\n", PduInfoPtr->SduPtr[1]
backtrace 8
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # LIN Frame ID = 0x10
        pid = parse_printf_value(gdb_output, "L1_LIN_PID")
        kq.them_kiem_tra("L1 LIN – Frame ID", "0x10", pid or "N/A",
                         pid == "0x10")

        # DL = 4
        dl = parse_printf_value(gdb_output, "L1_LIN_DL")
        kq.them_kiem_tra("L1 LIN – Data Length", "4", dl or "N/A",
                         dl == "4")

        # Data[0] = 0x03 (Headlamp + DRL)
        data0 = parse_printf_value(gdb_output, "L1_LIN_DATA0")
        kq.them_kiem_tra("L1 LIN – Data[0] (Headlamp+DRL xuyên 5 layer)", "0x03",
                         data0 or "N/A", data0 == "0x03")

        # Data[1] = 0x50 (Brightness=80)
        data1 = parse_printf_value(gdb_output, "L1_LIN_DATA1")
        kq.them_kiem_tra("L1 LIN – Data[1] (Brightness xuyên 5 layer)", "0x50",
                         data1 or "N/A", data1 == "0x50")

        # Backtrace
        bt = parse_backtrace(gdb_output)
        expected_funcs = ["Lin_SendFrame", "LinIf_MainFunction"]
        found = [f for f in expected_funcs if f in bt]
        kq.them_kiem_tra(
            "Call chain LinIf_MainFunction→Lin_SendFrame",
            "2 hàm", f"{len(found)} hàm",
            len(found) == 2,
        )

        return kq


test_case = TC12_E2E_LIN()
