#!/usr/bin/env python3
"""TC02: COM Signal Packing – EngineCmd (CAN 0x180)

Mô tả:
  Xác minh Com_SendSignal() pack đúng 5 signal vào Com_IpduBuf[0]:
    byte[0] = Throttle = 75 (0x4B)
    byte[1] = EngineStart = 1 (0x01)
    byte[2] = TorqueLimit = 200 (0xC8)
    byte[3] = Alive(4bit) | CRC(4bit) – dynamic

Phương pháp:
  Break tại Com_TriggerIPDUSend, đọc Com_IpduBuf[0] bằng x/5bx.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import ENGINE_CMD, parse_memory_bytes, kiem_tra_buffer


class TC02_COM_Engine(TestCaseCoBan):
    ma = "TC02"
    ten = "COM Signal Packing – EngineCmd"
    mo_ta = "Xác minh pack signal vào I-PDU buffer: Throttle=75, Start=1, TorqueLimit=200"

    def tao_gdb_script(self) -> str:
        return """
break Com_TriggerIPDUSend
continue
printf "TC02_PduId=%d\\n", PduId
x/5bx Com_IpduBuf
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # Parse memory bytes
        mem_bytes = parse_memory_bytes(gdb_output)

        if len(mem_bytes) < 3:
            kq.loi = f"Không đọc được buffer COM (chỉ có {len(mem_bytes)} byte)"
            return kq

        checks = kiem_tra_buffer(mem_bytes, ENGINE_CMD["com_buffer_fixed"])
        for idx, exp, act, match in checks:
            kq.them_kiem_tra(
                ten=f"Byte[{idx}]",
                mong_doi=f"0x{exp:02X}",
                thuc_te=f"0x{act:02X}" if act >= 0 else "N/A",
                dat=match,
            )

        return kq


test_case = TC02_COM_Engine()
