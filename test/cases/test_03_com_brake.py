#!/usr/bin/env python3
"""TC03: COM Signal Packing – BrakeCmd (CAN 0x280)

Mô tả:
  Xác minh Com_SendSignal() pack đúng:
    byte[0] = BrakeReq = 30 (0x1E)
    byte[1] = RegenReq = 50 (0x32)
    byte[2] = Alive(4bit) | CRC(4bit) – dynamic
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import BRAKE_CMD, parse_memory_bytes, kiem_tra_buffer


class TC03_COM_Brake(TestCaseCoBan):
    ma = "TC03"
    ten = "COM Signal Packing – BrakeCmd"
    mo_ta = "Xác minh pack signal vào I-PDU buffer: BrakeReq=30, RegenReq=50"

    def tao_gdb_script(self) -> str:
        return """
break Com_TriggerIPDUSend
continue
continue
printf "TC03_PduId=%d\\n", PduId
x/3bx (Com_IpduBuf + 1)
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        mem_bytes = parse_memory_bytes(gdb_output)
        if len(mem_bytes) < 2:
            kq.loi = f"Không đọc được buffer COM BrakeCmd (chỉ có {len(mem_bytes)} byte)"
            return kq

        checks = kiem_tra_buffer(mem_bytes, BRAKE_CMD["com_buffer_fixed"])
        for idx, exp, act, match in checks:
            kq.them_kiem_tra(
                ten=f"Byte[{idx}]",
                mong_doi=f"0x{exp:02X}",
                thuc_te=f"0x{act:02X}" if act >= 0 else "N/A",
                dat=match,
            )

        return kq


test_case = TC03_COM_Brake()
