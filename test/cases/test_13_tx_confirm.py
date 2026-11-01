#!/usr/bin/env python3
"""TC13: TX Confirmation Callback Chain

Mô tả:
  Xác minh chuỗi callback TX confirmation:
    Can_MainFunction_Write() → CanIf_TxConfirmation() →
    PduR_CanIfTxConfirmation() → Com_TxConfirmation()

Phương pháp:
  Break tại Com_TxConfirmation, kiểm tra backtrace chứa đầy đủ chain.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_backtrace, parse_printf_value


class TC13_TxConfirm(TestCaseCoBan):
    ma = "TC13"
    ten = "TX Confirmation Callback Chain"
    mo_ta = "Xác minh chuỗi callback: Can_MainFunc→CanIf_TxConf→PduR→Com_TxConf"

    def tao_gdb_script(self) -> str:
        return """
break Com_TxConfirmation
continue
printf "TC13_PduId=%d\\n", ComTxPduId
backtrace 8
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # PduId nhận được
        pdu_id = parse_printf_value(gdb_output, "TC13_PduId")
        kq.them_kiem_tra("Com_TxConfirmation được gọi", "có",
                         "có" if pdu_id is not None else "không",
                         pdu_id is not None)

        # Backtrace chain
        bt = parse_backtrace(gdb_output)
        expected_chain = [
            "Com_TxConfirmation",
            "PduR_CanIfTxConfirmation",
            "CanIf_TxConfirmation",
            "Can_MainFunction_Write",
        ]

        for func in expected_chain:
            found = func in bt
            kq.them_kiem_tra(
                f"Backtrace chứa {func}",
                "có", "có" if found else "không", found,
            )

        return kq


test_case = TC13_TxConfirm()
