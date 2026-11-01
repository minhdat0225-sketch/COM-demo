#!/usr/bin/env python3
"""TC07: PduR Routing – LIN path

Mô tả:
  Xác minh PduR dispatch đúng 2 LIN PDU (3,4) → LinIf_Transmit.
  Kiểm tra LinIf_Transmit được gọi 2 lần.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value


class TC07_PduR_LIN(TestCaseCoBan):
    ma = "TC07"
    ten = "PduR Routing – LIN path"
    mo_ta = "Xác minh PduR dispatch PDU 3,4 → LinIf_Transmit"

    def tao_gdb_script(self) -> str:
        return """
break LinIf_Transmit
continue
printf "LINIF_HIT_0_PduId=%d\\n", TxPduId
printf "LINIF_HIT_0_Len=%d\\n", PduInfoPtr->SduLength
backtrace 5
continue
printf "LINIF_HIT_1_PduId=%d\\n", TxPduId
printf "LINIF_HIT_1_Len=%d\\n", PduInfoPtr->SduLength
backtrace 5
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        expected = [
            {"name": "LightCtrl", "pdu_id": "0", "len": "4"},
            {"name": "HVACCtrl",  "pdu_id": "1", "len": "3"},
        ]

        for i, exp in enumerate(expected):
            actual_id = parse_printf_value(gdb_output, f"LINIF_HIT_{i}_PduId")
            actual_len = parse_printf_value(gdb_output, f"LINIF_HIT_{i}_Len")

            kq.them_kiem_tra(
                ten=f"LinIf hit {i} ({exp['name']}) – PduId",
                mong_doi=exp["pdu_id"],
                thuc_te=actual_id or "(không tìm thấy)",
                dat=actual_id == exp["pdu_id"],
            )
            kq.them_kiem_tra(
                ten=f"LinIf hit {i} ({exp['name']}) – DLC",
                mong_doi=exp["len"],
                thuc_te=actual_len or "(không tìm thấy)",
                dat=actual_len == exp["len"],
            )

        return kq


test_case = TC07_PduR_LIN()
