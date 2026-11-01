#!/usr/bin/env python3
"""TC05: COM Signal Packing – LightCtrl (LIN 0x10)

Mô tả:
  Xác minh pack signal cho LIN path:
    byte[0] bit0 = Headlamp = 1
    byte[0] bit1 = DRL = 1
    → byte[0] = 0x03
    byte[1] = Brightness = 80 (0x50)
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import LIGHT_CTRL, parse_memory_bytes, kiem_tra_buffer


class TC05_COM_Light(TestCaseCoBan):
    ma = "TC05"
    ten = "COM Signal Packing – LightCtrl (LIN)"
    mo_ta = "Xác minh pack signal LIN: Headlamp=1, DRL=1 → 0x03, Brightness=80 → 0x50"

    def tao_gdb_script(self) -> str:
        # LightCtrl is the 4th TriggerIPDUSend call
        return """
break Com_TriggerIPDUSend
continue
continue
continue
continue
printf "TC05_PduId=%d\\n", PduId
x/4bx (Com_IpduBuf + 3)
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        mem_bytes = parse_memory_bytes(gdb_output)
        if len(mem_bytes) < 2:
            kq.loi = f"Không đọc được buffer COM LightCtrl"
            return kq

        checks = kiem_tra_buffer(mem_bytes, LIGHT_CTRL["com_buffer_fixed"])
        for idx, exp, act, match in checks:
            kq.them_kiem_tra(
                ten=f"Byte[{idx}]",
                mong_doi=f"0x{exp:02X}",
                thuc_te=f"0x{act:02X}" if act >= 0 else "N/A",
                dat=match,
            )

        return kq


test_case = TC05_COM_Light()
