#!/usr/bin/env python3
"""TC04: COM Signal Packing – BodyCmd (CAN 0x380) – Bit fields

Mô tả:
  Xác minh bit packing cho 4 boolean signals:
    byte[0] bit0 = Headlamp = 1
    byte[0] bit1 = TurnL = 1
    byte[0] bit2 = TurnR = 0
    byte[0] bit3 = DoorLock = 1
    → byte[0] = 0b00001011 = 0x0B
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import BODY_CMD, parse_memory_bytes, kiem_tra_buffer


class TC04_COM_Body(TestCaseCoBan):
    ma = "TC04"
    ten = "COM Signal Packing – BodyCmd (bit-field)"
    mo_ta = "Xác minh bit packing: Headlamp=1, TurnL=1, TurnR=0, DoorLock=1 → byte[0]=0x0B"

    def tao_gdb_script(self) -> str:
        # BodyCmd is the 3rd TriggerIPDUSend call → continue 3 times
        return """
break Com_TriggerIPDUSend
continue
continue
continue
printf "TC04_PduId=%d\\n", PduId
x/4bx (Com_IpduBuf + 2)
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        mem_bytes = parse_memory_bytes(gdb_output)
        if not mem_bytes:
            kq.loi = "Không đọc được buffer COM BodyCmd"
            return kq

        checks = kiem_tra_buffer(mem_bytes, BODY_CMD["com_buffer_fixed"])
        for idx, exp, act, match in checks:
            kq.them_kiem_tra(
                ten=f"Byte[{idx}] (bit fields)",
                mong_doi=f"0x{exp:02X} (b{exp:08b})",
                thuc_te=f"0x{act:02X} (b{act:08b})" if act >= 0 else "N/A",
                dat=match,
            )

        return kq


test_case = TC04_COM_Body()
