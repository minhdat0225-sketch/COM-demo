#!/usr/bin/env python3
"""TC16: End-to-End LIN Rx TempSensor

Mô tả:
  Truy vết luồng nhận dữ liệu LIN Rx từ Lin_SendFrame (Header) lên COM:
    Layer 1 (LIN Driver): Lin_SendFrame được gọi với Drc=1 (RX).
                          GDB mock nhận dữ liệu: s_linRxBuf[0]=0x23 (35°C), s_linRxBuf[1]=0x01 (Status OK), s_linStatus=3 (LIN_TX_OK).
    Layer 2 (LinIf):      LinIf_MainFunction gọi PduR_LinIfRxIndication với data mới.
    Layer 3 (PduR):       PduR định tuyến sang Com_RxIndication.
    Layer 4 (COM):        Com_RxIndication copy dữ liệu vào shadow buffer Com_IpduBuf[6].
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value, parse_backtrace


class TC16_E2E_LIN_RX(TestCaseCoBan):
    ma = "TC16"
    ten = "End-to-End LIN Rx TempSensor"
    mo_ta = "Truy vết nhận dữ liệu LIN TempSensor qua 4 layer: LIN Driver→LinIf→PduR→COM"

    def tao_gdb_script(self) -> str:
        return """
# 1. Break tại Lin_SendFrame khi gửi LIN RX Header
break Lin_SendFrame if PduInfoPtr->Drc == 1
continue
printf "L1_LIN_PID=0x%02x\\n", PduInfoPtr->Pid
printf "L1_LIN_DL=%d\\n", PduInfoPtr->Dl

# Mock nhận dữ liệu từ cảm biến
set s_linRxBuf[0] = 0x23
set s_linRxBuf[1] = 0x01
set s_linStatus = 3
return 0

# 2. Break tại PduR_LinIfRxIndication để kiểm tra BSW Routing
break PduR_LinIfRxIndication
continue
printf "L3_PDUR_PDU_ID=%d\\n", LinIfRxPduId
printf "L3_PDUR_DATA0=0x%02x\\n", PduInfoPtr->SduDataPtr[0]
printf "L3_PDUR_DATA1=0x%02x\\n", PduInfoPtr->SduDataPtr[1]

# 3. Break tại Com_RxIndication để kiểm tra COM Layer nhận
break Com_RxIndication
continue
printf "L4_COM_PDU_ID=%d\\n", ComRxPduId
printf "L4_COM_DATA0=0x%02x\\n", PduInfoPtr->SduDataPtr[0]
printf "L4_COM_DATA1=0x%02x\\n", PduInfoPtr->SduDataPtr[1]

backtrace 5
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        # L1: LIN Frame ID = 0x20
        pid = parse_printf_value(gdb_output, "L1_LIN_PID")
        kq.them_kiem_tra("L1 LIN Driver – Frame ID (TempSensor)", "0x20", pid or "N/A",
                         pid == "0x20")

        # L1: DL = 2
        dl = parse_printf_value(gdb_output, "L1_LIN_DL")
        kq.them_kiem_tra("L1 LIN Driver – DLC (2 bytes)", "2", dl or "N/A",
                         dl == "2")

        # L3: PduR PDU ID = 2
        pdur_id = parse_printf_value(gdb_output, "L3_PDUR_PDU_ID")
        kq.them_kiem_tra("L3 PduR – LinIfRxPduId (2)", "2", pdur_id or "N/A",
                         pdur_id == "2")

        # L3: Data verification
        pdur_d0 = parse_printf_value(gdb_output, "L3_PDUR_DATA0")
        pdur_d1 = parse_printf_value(gdb_output, "L3_PDUR_DATA1")
        kq.them_kiem_tra("L3 PduR – Data[0] (Temp Value)", "0x23", pdur_d0 or "N/A",
                         pdur_d0 == "0x23")
        kq.them_kiem_tra("L3 PduR – Data[1] (Temp Status)", "0x01", pdur_d1 or "N/A",
                         pdur_d1 == "0x01")

        # L4: COM PduId = 6
        com_id = parse_printf_value(gdb_output, "L4_COM_PDU_ID")
        kq.them_kiem_tra("L4 COM – ComRxPduId (6)", "6", com_id or "N/A",
                         com_id == "6")

        # L4: Data verification
        com_d0 = parse_printf_value(gdb_output, "L4_COM_DATA0")
        com_d1 = parse_printf_value(gdb_output, "L4_COM_DATA1")
        kq.them_kiem_tra("L4 COM – Data[0] (Temp Value)", "0x23", com_d0 or "N/A",
                         com_d0 == "0x23")
        kq.them_kiem_tra("L4 COM – Data[1] (Temp Status)", "0x01", com_d1 or "N/A",
                         com_d1 == "0x01")

        # Check call stack
        bt = parse_backtrace(gdb_output)
        expected_chain = ["Com_RxIndication", "PduR_LinIfRxIndication", "LinIf_MainFunction"]
        found_count = sum(1 for f in expected_chain if f in bt)
        kq.them_kiem_tra(
            "Call chain LinIf_MainFunction→PduR_LinIfRxIndication→Com_RxIndication",
            "3 hàm", f"{found_count} hàm",
            found_count == 3
        )

        return kq


test_case = TC16_E2E_LIN_RX()
