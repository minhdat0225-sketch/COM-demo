#!/usr/bin/env python3
"""TC15: PduR Uninit Guard

Mô tả:
  Xác minh PduR_Init thiết lập trạng thái ONLINE
  và routing được bật (s_RoutingEnabled = TRUE).

Phương pháp:
  Break ngay sau PduR_Init, kiểm tra trạng thái nội bộ.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value


class TC15_PduR_State(TestCaseCoBan):
    ma = "TC15"
    ten = "PduR trạng thái sau Init"
    mo_ta = "Xác minh PduR_Init → state=ONLINE, routing=enabled"

    def tao_gdb_script(self) -> str:
        return """
break PduR_Init
continue
finish
printf "TC15_State=%d\\n", s_State
printf "TC15_Routing=%d\\n", s_RoutingEnabled
printf "TC15_CfgValid=%d\\n", (s_Cfg != 0)
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # s_State = PDUR_ONLINE = 1
        state = parse_printf_value(gdb_output, "TC15_State")
        kq.them_kiem_tra("PduR State = ONLINE", "1", state or "N/A",
                         state == "1")

        # s_RoutingEnabled = TRUE = 1
        routing = parse_printf_value(gdb_output, "TC15_Routing")
        kq.them_kiem_tra("Routing Enabled", "1", routing or "N/A",
                         routing == "1")

        # s_Cfg != NULL
        cfg = parse_printf_value(gdb_output, "TC15_CfgValid")
        kq.them_kiem_tra("Config pointer hợp lệ", "1", cfg or "N/A",
                         cfg == "1")

        return kq


test_case = TC15_PduR_State()
