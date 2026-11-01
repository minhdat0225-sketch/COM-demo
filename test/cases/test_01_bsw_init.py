#!/usr/bin/env python3
"""TC01: Kiểm tra thứ tự khởi tạo BSW Stack

Mô tả:
  Xác minh BSW init theo đúng thứ tự AUTOSAR (bottom-up):
    Can_Init → CanIf_Init → LinIf_Init → Lin_Init → PduR_Init → Com_Init

Phương pháp:
  Đặt breakpoint tại tất cả hàm Init, continue qua từng cái,
  ghi nhận thứ tự hit.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import BSW_INIT_ORDER, parse_bp_hits


class TC01_BSW_Init(TestCaseCoBan):
    ma = "TC01"
    ten = "Kiểm tra thứ tự khởi tạo BSW"
    mo_ta = "Xác minh init đúng thứ tự: Can→CanIf→LinIf(→Lin)→PduR→Com"

    def tao_gdb_script(self) -> str:
        cmds = ""
        for func in BSW_INIT_ORDER:
            cmds += f"break {func}\n"

        for i, func in enumerate(BSW_INIT_ORDER):
            cmds += "continue\n"
            cmds += f'printf "BP_HIT_{i}={func}\\n"\n'
            cmds += "backtrace 3\n"

        return cmds

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(
            ma=self.ma, ten=self.ten, mo_ta=self.mo_ta
        )

        hits = parse_bp_hits(gdb_output)

        # Kiểm tra đủ 6 breakpoint
        kq.them_kiem_tra(
            ten="Số lượng Init hit",
            mong_doi=str(len(BSW_INIT_ORDER)),
            thuc_te=str(len(hits)),
            dat=len(hits) == len(BSW_INIT_ORDER),
        )

        # Kiểm tra thứ tự từng hàm
        for i, expected_func in enumerate(BSW_INIT_ORDER):
            actual = hits[i] if i < len(hits) else "(không có)"
            kq.them_kiem_tra(
                ten=f"Init thứ {i+1}",
                mong_doi=expected_func,
                thuc_te=actual,
                dat=actual == expected_func,
            )

        return kq


# Export cho runner tự phát hiện
test_case = TC01_BSW_Init()
