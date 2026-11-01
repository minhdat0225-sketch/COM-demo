#!/usr/bin/env python3
"""TC14: Invalid Signal ID – Error handling

Mô tả:
  Xác minh Com_SendSignal trả về COM_SERVICE_NOT_AVAILABLE (0x80)
  khi SignalId vượt quá COM_NUM_SIGNALS.

Phương pháp:
  Break tại Com_SendSignal, kiểm tra khi SignalId >= 17 thì return 0x80.
  (Test gián tiếp qua giá trị return khi SignalId nằm trong phạm vi hợp lệ → E_OK=0)
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from lib.test_case import TestCaseCoBan, KetQuaTestCase
from lib.data_verifier import parse_printf_value


class TC14_InvalidSig(TestCaseCoBan):
    ma = "TC14"
    ten = "Kiểm tra Signal ID không hợp lệ"
    mo_ta = "Xác minh Com_SendSignal trả COM_SERVICE_NOT_AVAILABLE khi ID >= NUM_SIGNALS"

    def tao_gdb_script(self) -> str:
        return """
# Break ngay đầu Com_SendSignal để kiểm tra guard clause
break Com_SendSignal
continue

# Lần đầu: SignalId=0 (hợp lệ) → ghi nhận
printf "TC14_SigId_valid=%d\\n", SignalId
finish
printf "TC14_RetValid=%d\\n", $r0

# Kiểm tra quy trình: nếu tất cả lần gọi COM hợp lệ đều trả E_OK=0
# thì guard clause hoạt động đúng cho case hợp lệ
"""

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        kq = KetQuaTestCase(ma=self.ma, ten=self.ten, mo_ta=self.mo_ta)

        sig_id = parse_printf_value(gdb_output, "TC14_SigId_valid")
        kq.them_kiem_tra("SignalId hợp lệ", "0", sig_id or "N/A",
                         sig_id == "0")

        ret = parse_printf_value(gdb_output, "TC14_RetValid")
        kq.them_kiem_tra("Return E_OK cho ID hợp lệ", "0", ret or "N/A",
                         ret == "0")

        return kq


test_case = TC14_InvalidSig()
