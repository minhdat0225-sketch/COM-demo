#!/usr/bin/env python3
"""TC10: CAN Driver HW Write

Mô tả:
  Xác minh Can_Write tạo đúng CanTxMsg (SPL struct) và gọi CAN_Transmit:
    - StdId = 0x180
    - DLC = 5
    - IDE = Standard
    - Data[0] = Throttle = 75
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value


class TC10_CAN_Driver(TestCaseCoBan):
    ma = "TC10"
    ten = "CAN Driver – CanTxMsg"
    mo_ta = "Xác minh Can_Write dựng đúng CanTxMsg: StdId=0x180, DLC=5, Data[0]=75"

    def tao_gdb_script(self) -> str:
        # Break after txMsg is fully constructed, just before CAN_Transmit
        return """
break Can.c:301
continue
printf "TC10_StdId=0x%x\\n", txMsg.StdId
printf "TC10_ExtId=0x%x\\n", txMsg.ExtId
printf "TC10_DLC=%d\\n", txMsg.DLC
printf "TC10_IDE=%d\\n", txMsg.IDE
printf "TC10_RTR=%d\\n", txMsg.RTR
printf "TC10_Data0=%d\\n", txMsg.Data[0]
printf "TC10_Data1=%d\\n", txMsg.Data[1]
printf "TC10_Data2=%d\\n", txMsg.Data[2]
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # StdId
        std_id = parse_printf_value(gdb_output, "TC10_StdId")
        kq.them_kiem_tra("StdId", "0x180", std_id or "N/A",
                         std_id == "0x180")

        # DLC
        dlc = parse_printf_value(gdb_output, "TC10_DLC")
        kq.them_kiem_tra("DLC", "5", dlc or "N/A",
                         dlc == "5")

        # IDE = Standard = 0
        ide = parse_printf_value(gdb_output, "TC10_IDE")
        kq.them_kiem_tra("IDE (Standard=0)", "0", ide or "N/A",
                         ide == "0")

        # RTR = Data = 0
        rtr = parse_printf_value(gdb_output, "TC10_RTR")
        kq.them_kiem_tra("RTR (Data=0)", "0", rtr or "N/A",
                         rtr == "0")

        # Data[0] = Throttle = 75
        data0 = parse_printf_value(gdb_output, "TC10_Data0")
        kq.them_kiem_tra("Data[0] (Throttle)", "75", data0 or "N/A",
                         data0 == "75")

        # Data[1] = EngineStart = 1
        data1 = parse_printf_value(gdb_output, "TC10_Data1")
        kq.them_kiem_tra("Data[1] (EngineStart)", "1", data1 or "N/A",
                         data1 == "1")

        # Data[2] = TorqueLimit = 200
        data2 = parse_printf_value(gdb_output, "TC10_Data2")
        kq.them_kiem_tra("Data[2] (TorqueLimit)", "200", data2 or "N/A",
                         data2 == "200")

        return kq


test_case = TC10_CAN_Driver()
