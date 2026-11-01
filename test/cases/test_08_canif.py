#!/usr/bin/env python3
"""TC08: CanIf Data Transform

Mô tả:
  Xác minh CanIf_Transmit tạo đúng Can_PduType:
    - EngineCmd: id=0x180, length=5, swPduHandle=0
  Kiểm tra tại hàm Can_Write().
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import ENGINE_CMD, parse_printf_value, parse_backtrace


class TC08_CanIf(TestCaseCoBan):
    ma = "TC08"
    ten = "CanIf Data Transform"
    mo_ta = "Xác minh CanIf tạo đúng Can_PduType: CAN ID=0x180, DLC=5, HTH=0"

    def tao_gdb_script(self) -> str:
        return """
break Can_Write
continue
printf "TC08_HTH=%d\\n", Hth
printf "TC08_CAN_ID=0x%x\\n", PduInfo->id
printf "TC08_DLC=%d\\n", PduInfo->length
printf "TC08_HANDLE=%d\\n", PduInfo->swPduHandle
printf "TC08_DATA0=0x%02x\\n", PduInfo->sdu[0]
printf "TC08_DATA1=0x%02x\\n", PduInfo->sdu[1]
printf "TC08_DATA2=0x%02x\\n", PduInfo->sdu[2]
backtrace 5
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # Kiểm tra HTH
        hth = parse_printf_value(gdb_output, "TC08_HTH")
        kq.them_kiem_tra("HTH (mailbox)", "0", hth or "N/A",
                         hth == "0")

        # Kiểm tra CAN ID
        can_id = parse_printf_value(gdb_output, "TC08_CAN_ID")
        kq.them_kiem_tra("CAN ID", "0x180", can_id or "N/A",
                         can_id == "0x180")

        # Kiểm tra DLC
        dlc = parse_printf_value(gdb_output, "TC08_DLC")
        kq.them_kiem_tra("DLC", "5", dlc or "N/A",
                         dlc == "5")

        # Kiểm tra swPduHandle
        handle = parse_printf_value(gdb_output, "TC08_HANDLE")
        kq.them_kiem_tra("swPduHandle", "0", handle or "N/A",
                         handle == "0")

        # Kiểm tra data (Throttle = 75 = 0x4b)
        data0 = parse_printf_value(gdb_output, "TC08_DATA0")
        kq.them_kiem_tra("Data[0] (Throttle)", "0x4b", data0 or "N/A",
                         data0 == "0x4b")

        # Kiểm tra backtrace có đúng call chain
        bt = parse_backtrace(gdb_output)
        has_chain = ("Can_Write" in bt and
                     any("CanIf" in f for f in bt))
        kq.them_kiem_tra("Call chain CanIf→Can_Write", "có", "có" if has_chain else "không",
                         has_chain)

        return kq


test_case = TC08_CanIf()
