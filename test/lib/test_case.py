#!/usr/bin/env python3
"""
test_case.py – Base class cho tất cả test case AUTOSAR COM Stack

Cung cấp:
  - Cấu trúc chung: tên, mô tả, kết quả
  - Phương thức chay(): orchestrate setup → execute → verify
  - Logging tiếng Việt
"""

import time
from enum import Enum
from dataclasses import dataclass, field
from typing import List


class KetQua(Enum):
    """Kết quả test case."""
    DAT = "ĐẠT"
    KHONG_DAT = "KHÔNG ĐẠT"
    LOI = "LỖI"
    BO_QUA = "BỎ QUA"


@dataclass
class ChiTietKiemTra:
    """Chi tiết một kiểm tra con trong test case."""
    ten: str
    mong_doi: str
    thuc_te: str
    dat: bool

    def __str__(self):
        icon = "✓" if self.dat else "✗"
        return f"  {icon} {self.ten}: mong đợi={self.mong_doi}, thực tế={self.thuc_te}"


@dataclass
class KetQuaTestCase:
    """Kết quả tổng hợp của một test case."""
    ma: str                                    # TC01, TC02, ...
    ten: str
    mo_ta: str
    ket_qua: KetQua = KetQua.BO_QUA
    thoi_gian_ms: float = 0.0
    chi_tiet: List[ChiTietKiemTra] = field(default_factory=list)
    gdb_output: str = ""
    loi: str = ""

    @property
    def dat(self) -> bool:
        return self.ket_qua == KetQua.DAT

    def them_kiem_tra(self, ten: str, mong_doi, thuc_te, dat: bool):
        """Thêm một kiểm tra con."""
        self.chi_tiet.append(ChiTietKiemTra(
            ten=ten,
            mong_doi=str(mong_doi),
            thuc_te=str(thuc_te),
            dat=dat,
        ))

    def bao_cao(self) -> str:
        """Tạo báo cáo text cho test case này."""
        icon = "✓ ĐẠT" if self.dat else "✗ KHÔNG ĐẠT"
        lines = [
            f"[{icon}] {self.ma}: {self.ten} ({self.thoi_gian_ms:.1f}ms)",
            f"       Mô tả: {self.mo_ta}",
        ]
        for ct in self.chi_tiet:
            lines.append(str(ct))
        if self.loi:
            lines.append(f"  ⚠ Lỗi: {self.loi}")
        return "\n".join(lines)


class TestCaseCoBan:
    """Base class cho mỗi test case.

    Subclass phải override:
      - ma, ten, mo_ta (class attributes)
      - tao_gdb_script() → str: GDB commands
      - kiem_tra(result) → KetQuaTestCase
    """

    ma: str = "TC00"
    ten: str = "Test cơ bản"
    mo_ta: str = "Mô tả test case"

    def tao_gdb_script(self) -> str:
        """Tạo nội dung GDB script cho test case.

        Returns:
            Multi-line string chứa các lệnh GDB.
        """
        raise NotImplementedError

    def kiem_tra(self, gdb_output: str) -> KetQuaTestCase:
        """Phân tích output GDB và trả về kết quả.

        Args:
            gdb_output: Toàn bộ stdout+stderr từ GDB batch.

        Returns:
            KetQuaTestCase với các chi tiết kiểm tra.
        """
        raise NotImplementedError

    def chay(self, gdb_controller) -> KetQuaTestCase:
        """Chạy test case: sinh script → chạy GDB → kiểm tra.

        Args:
            gdb_controller: Instance GdbController đã kết nối.

        Returns:
            KetQuaTestCase.
        """
        t0 = time.time()
        try:
            script = self.tao_gdb_script()
            result = gdb_controller.chay_script(script, timeout=30)

            kq = self.kiem_tra(result.output)
            kq.gdb_output = result.output
            kq.thoi_gian_ms = (time.time() - t0) * 1000

            # Xác định pass/fail tổng hợp
            if kq.loi:
                kq.ket_qua = KetQua.LOI
            elif all(ct.dat for ct in kq.chi_tiet):
                kq.ket_qua = KetQua.DAT
            else:
                kq.ket_qua = KetQua.KHONG_DAT

            return kq

        except Exception as e:
            kq = KetQuaTestCase(
                ma=self.ma, ten=self.ten, mo_ta=self.mo_ta,
                ket_qua=KetQua.LOI,
                thoi_gian_ms=(time.time() - t0) * 1000,
                loi=str(e),
            )
            return kq
