#!/usr/bin/env python3
"""TC09: LinIf Buffer & MainFunction

Mô tả:
  Xác minh LinIf_Transmit lưu dữ liệu vào pending buffer,
  và LinIf_MainFunction gọi Lin_SendFrame đúng.

Phương pháp:
  Break tại Lin_SendFrame, kiểm tra tham số Lin_PduType.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import LIGHT_CTRL, parse_printf_value, parse_backtrace


class TC09_LinIf(TestCaseCoBan):
    ma = "TC09"
    ten = "LinIf Buffer & MainFunction"
    mo_ta = "Xác minh LinIf buffer pending → Lin_SendFrame với LIN ID=0x10, DL=4"

    def tao_gdb_script(self) -> str:
        return """
break Lin_SendFrame
continue
printf "TC09_CHANNEL=%d\\n", Channel
printf "TC09_PID=0x%02x\\n", PduInfoPtr->Pid
printf "TC09_DL=%d\\n", PduInfoPtr->Dl
printf "TC09_CS=%d\\n", PduInfoPtr->Cs
printf "TC09_DRC=%d\\n", PduInfoPtr->Drc
printf "TC09_DATA0=0x%02x\\n", PduInfoPtr->SduPtr[0]
printf "TC09_DATA1=0x%02x\\n", PduInfoPtr->SduPtr[1]
backtrace 5
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # LIN frame ID
        pid = parse_printf_value(gdb_output, "TC09_PID")
        kq.them_kiem_tra("LIN Frame ID", "0x10", pid or "N/A",
                         pid == "0x10")

        # Data length
        dl = parse_printf_value(gdb_output, "TC09_DL")
        kq.them_kiem_tra("Data Length", "4", dl or "N/A",
                         dl == "4")

        # Checksum type: Enhanced = 0
        cs = parse_printf_value(gdb_output, "TC09_CS")
        kq.them_kiem_tra("Checksum type (Enhanced)", "0", cs or "N/A",
                         cs == "0")

        # Direction: TX = 0
        drc = parse_printf_value(gdb_output, "TC09_DRC")
        kq.them_kiem_tra("Frame response (TX)", "0", drc or "N/A",
                         drc == "0")

        # Data[0] = 0x03 (Headlamp=1, DRL=1)
        data0 = parse_printf_value(gdb_output, "TC09_DATA0")
        kq.them_kiem_tra("Data[0] (Headlamp+DRL)", "0x03", data0 or "N/A",
                         data0 == "0x03")

        # Data[1] = 0x50 (Brightness=80)
        data1 = parse_printf_value(gdb_output, "TC09_DATA1")
        kq.them_kiem_tra("Data[1] (Brightness)", "0x50", data1 or "N/A",
                         data1 == "0x50")

        # Call chain
        bt = parse_backtrace(gdb_output)
        has_chain = "Lin_SendFrame" in bt and "LinIf_MainFunction" in bt
        kq.them_kiem_tra("Call chain LinIf_MainFunction→Lin_SendFrame",
                         "có", "có" if has_chain else "không", has_chain)

        return kq


test_case = TC09_LinIf()
