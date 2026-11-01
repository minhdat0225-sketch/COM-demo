#!/usr/bin/env python3
"""TC06: PduR Routing – CAN path

Mô tả:
  Xác minh PduR dispatch đúng 3 CAN PDU (0,1,2) → CanIf_Transmit.
  Kiểm tra CanIf_Transmit được gọi 3 lần với đúng PduId.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value


class TC06_PduR_CAN(TestCaseCoBan):
    ma = "TC06"
    ten = "PduR Routing – CAN path"
    mo_ta = "Xác minh PduR dispatch PDU 0,1,2 → CanIf_Transmit với đúng PduId đích"

    def tao_gdb_script(self) -> str:
        return """
break CanIf_Transmit
continue
printf "CANIF_HIT_0_PduId=%d\\n", CanIfTxPduId
printf "CANIF_HIT_0_Len=%d\\n", PduInfoPtr->SduLength
continue
printf "CANIF_HIT_1_PduId=%d\\n", CanIfTxPduId
printf "CANIF_HIT_1_Len=%d\\n", PduInfoPtr->SduLength
continue
printf "CANIF_HIT_2_PduId=%d\\n", CanIfTxPduId
printf "CANIF_HIT_2_Len=%d\\n", PduInfoPtr->SduLength
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        expected = [
            {"name": "EngineCmd", "pdu_id": "0", "len": "5"},
            {"name": "BrakeCmd",  "pdu_id": "1", "len": "3"},
            {"name": "BodyCmd",   "pdu_id": "2", "len": "4"},
        ]

        for i, exp in enumerate(expected):
            actual_id = parse_printf_value(gdb_output, f"CANIF_HIT_{i}_PduId")
            actual_len = parse_printf_value(gdb_output, f"CANIF_HIT_{i}_Len")

            kq.them_kiem_tra(
                ten=f"CanIf hit {i} ({exp['name']}) – PduId",
                mong_doi=exp["pdu_id"],
                thuc_te=actual_id or "(không tìm thấy)",
                dat=actual_id == exp["pdu_id"],
            )
            kq.them_kiem_tra(
                ten=f"CanIf hit {i} ({exp['name']}) – DLC",
                mong_doi=exp["len"],
                thuc_te=actual_len or "(không tìm thấy)",
                dat=actual_len == exp["len"],
            )

        return kq


test_case = TC06_PduR_CAN()
