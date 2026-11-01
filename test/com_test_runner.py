#!/usr/bin/env python3
"""
com_test_runner.py – Bộ chạy test tự động AUTOSAR COM Stack

Sử dụng:
  python3 test/com_test_runner.py                  # Chạy tất cả test
  python3 test/com_test_runner.py --tc 01          # Chạy TC01
  python3 test/com_test_runner.py --tc 01,02,11    # Chạy TC01, TC02, TC11
  python3 test/com_test_runner.py --verbose        # Hiển thị GDB output
  python3 test/com_test_runner.py --no-build       # Bỏ qua bước build

Yêu cầu:
  - arm-none-eabi-gcc, arm-none-eabi-gdb
  - renode (đã cài và trong PATH)
  - Python 3.8+
"""

import argparse
import importlib
import json
import os
import subprocess
import sys
import time
from datetime import datetime

# Đảm bảo import được từ thư mục test/
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, SCRIPT_DIR)

from lib.renode_manager import RenodeManager
from lib.gdb_controller import GdbController
from lib.test_case import KetQua


# ========================================================================
# MÀU SẮC TERMINAL
# ========================================================================
class Mau:
    XANH = "\033[0;32m"
    DO = "\033[0;31m"
    VANG = "\033[0;33m"
    XANH_NHAT = "\033[0;36m"
    DAM = "\033[1m"
    RESET = "\033[0m"

    @staticmethod
    def tat():
        Mau.XANH = Mau.DO = Mau.VANG = Mau.XANH_NHAT = Mau.DAM = Mau.RESET = ""


# ========================================================================
# TIỆN ÍCH IN ẤN
# ========================================================================
def in_tieu_de():
    print()
    print(f"{Mau.XANH_NHAT}╔══════════════════════════════════════════════════════════╗{Mau.RESET}")
    print(f"{Mau.XANH_NHAT}║  {Mau.DAM}AUTOSAR COM Stack – Bộ Test Tự Động (Python+GDB+Renode){Mau.RESET}{Mau.XANH_NHAT} ║{Mau.RESET}")
    print(f"{Mau.XANH_NHAT}╚══════════════════════════════════════════════════════════╝{Mau.RESET}")
    print()


def in_thong_tin(msg):
    print(f"{Mau.XANH_NHAT}[THÔNG TIN]{Mau.RESET} {msg}")


def in_dat(msg):
    print(f"{Mau.XANH}[✓ ĐẠT]{Mau.RESET}    {msg}")


def in_khong_dat(msg):
    print(f"{Mau.DO}[✗ KHÔNG ĐẠT]{Mau.RESET} {msg}")


def in_loi(msg):
    print(f"{Mau.DO}[⚠ LỖI]{Mau.RESET}    {msg}")


def in_canh_bao(msg):
    print(f"{Mau.VANG}[CẢNH BÁO]{Mau.RESET} {msg}")


# ========================================================================
# TÌM VÀ NẠP TEST CASES
# ========================================================================
def tim_test_cases(filter_tc=None):
    """Tìm và nạp tất cả test case từ thư mục cases/.

    Args:
        filter_tc: Danh sách số TC cần chạy (vd: ["01", "02"]), hoặc None = tất cả.

    Returns:
        Danh sách test case instances.
    """
    cases_dir = os.path.join(SCRIPT_DIR, "cases")
    test_cases = []

    for filename in sorted(os.listdir(cases_dir)):
        if not filename.startswith("test_") or not filename.endswith(".py"):
            continue

        # Trích số TC từ tên file: test_01_xxx.py → "01"
        parts = filename.replace(".py", "").split("_")
        if len(parts) >= 2:
            tc_num = parts[1]
        else:
            continue

        # Lọc nếu cần
        if filter_tc and tc_num not in filter_tc:
            continue

        # Import module
        module_name = filename.replace(".py", "")
        try:
            module = importlib.import_module(f"cases.{module_name}")
            if hasattr(module, "test_case"):
                test_cases.append(module.test_case)
        except Exception as e:
            in_canh_bao(f"Không thể nạp {filename}: {e}")

    return test_cases


# ========================================================================
# BUILD PROJECT
# ========================================================================
def build_project():
    """Build project bằng make."""
    in_thong_tin("Biên dịch dự án (make clean && make)...")
    results_dir = os.path.join(SCRIPT_DIR, "results")
    os.makedirs(results_dir, exist_ok=True)
    build_log = os.path.join(results_dir, "build.log")

    # Clean
    subprocess.run(["make", "clean"], capture_output=True, cwd=PROJECT_DIR)

    # Build
    result = subprocess.run(
        ["make"], capture_output=True, text=True, cwd=PROJECT_DIR
    )

    with open(build_log, "w") as f:
        f.write(result.stdout)
        f.write(result.stderr)

    if result.returncode == 0:
        in_dat("Biên dịch thành công")
        return True
    else:
        in_khong_dat("Biên dịch thất bại!")
        print(f"       Xem chi tiết: {build_log}")
        print(result.stderr[-500:] if result.stderr else "")
        return False


# ========================================================================
# KIỂM TRA PHỤ THUỘC
# ========================================================================
def kiem_tra_phu_thuoc():
    """Kiểm tra các công cụ cần thiết đã cài chưa."""
    tools = {
        "arm-none-eabi-gcc": "Trình biên dịch ARM",
        "arm-none-eabi-gdb": "Trình gỡ lỗi GDB",
        "renode": "Bộ mô phỏng Renode",
    }

    all_ok = True
    for cmd, desc in tools.items():
        try:
            subprocess.run(
                [cmd, "--version"], capture_output=True, timeout=5
            )
            in_dat(f"{desc} ({cmd}) – đã cài đặt")
        except (FileNotFoundError, subprocess.TimeoutExpired):
            in_khong_dat(f"{desc} ({cmd}) – CHƯA CÀI ĐẶT")
            all_ok = False

    return all_ok


# ========================================================================
# TẠO BÁO CÁO
# ========================================================================
def tao_bao_cao_text(ket_qua_list, thoi_gian_tong) -> str:
    """Tạo báo cáo text tiếng Việt."""
    lines = []
    lines.append("═" * 60)
    lines.append("  BÁO CÁO TEST AUTOSAR COM STACK")
    lines.append(f"  Thời gian: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(f"  Thời gian chạy: {thoi_gian_tong:.1f} giây")
    lines.append("═" * 60)
    lines.append("")

    for kq in ket_qua_list:
        lines.append(kq.bao_cao())
        lines.append("")

    # Tổng kết
    tong = len(ket_qua_list)
    dat = sum(1 for kq in ket_qua_list if kq.dat)
    khong_dat = tong - dat

    lines.append("═" * 60)
    lines.append("               TỔNG KẾT")
    lines.append("═" * 60)
    lines.append(f"  Tổng số test  : {tong}")
    lines.append(f"  ĐẠT           : {dat}")
    lines.append(f"  KHÔNG ĐẠT     : {khong_dat}")
    lines.append("═" * 60)

    if khong_dat > 0:
        lines.append("")
        lines.append("  Các test KHÔNG ĐẠT:")
        for kq in ket_qua_list:
            if not kq.dat:
                lines.append(f"    - {kq.ma}: {kq.ten}")
                if kq.loi:
                    lines.append(f"      Lỗi: {kq.loi}")

    return "\n".join(lines)


def tao_bao_cao_json(ket_qua_list, thoi_gian_tong) -> dict:
    """Tạo báo cáo JSON."""
    return {
        "thoi_gian": datetime.now().isoformat(),
        "thoi_gian_chay_giay": round(thoi_gian_tong, 2),
        "tong_ket": {
            "tong": len(ket_qua_list),
            "dat": sum(1 for kq in ket_qua_list if kq.dat),
            "khong_dat": sum(1 for kq in ket_qua_list if not kq.dat),
        },
        "chi_tiet": [
            {
                "ma": kq.ma,
                "ten": kq.ten,
                "mo_ta": kq.mo_ta,
                "ket_qua": kq.ket_qua.value,
                "thoi_gian_ms": round(kq.thoi_gian_ms, 1),
                "loi": kq.loi,
                "kiem_tra": [
                    {
                        "ten": ct.ten,
                        "mong_doi": ct.mong_doi,
                        "thuc_te": ct.thuc_te,
                        "dat": ct.dat,
                    }
                    for ct in kq.chi_tiet
                ],
            }
            for kq in ket_qua_list
        ],
    }


# ========================================================================
# MAIN
# ========================================================================
def main():
    parser = argparse.ArgumentParser(
        description="Bộ test tự động AUTOSAR COM Stack (Python + GDB + Renode)"
    )
    parser.add_argument(
        "--tc", type=str, default=None,
        help="Số TC cần chạy, cách nhau bởi dấu phẩy (vd: 01,02,11). Mặc định: tất cả"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Hiển thị GDB output chi tiết"
    )
    parser.add_argument(
        "--no-build", action="store_true",
        help="Bỏ qua bước biên dịch (dùng ELF hiện có)"
    )
    parser.add_argument(
        "--no-color", action="store_true",
        help="Tắt màu sắc terminal"
    )
    parser.add_argument(
        "--gdb-port", type=int, default=3333,
        help="Port GDB (mặc định: 3333)"
    )
    args = parser.parse_args()

    if args.no_color:
        Mau.tat()

    in_tieu_de()

    # 1. Kiểm tra phụ thuộc
    in_thong_tin("Kiểm tra công cụ cần thiết...")
    if not kiem_tra_phu_thuoc():
        in_loi("Thiếu công cụ, không thể tiếp tục!")
        sys.exit(1)
    print()

    # 2. Build
    if not args.no_build:
        if not build_project():
            sys.exit(1)
        print()

    # 3. Kiểm tra ELF
    elf_path = os.path.join(PROJECT_DIR, "com_demo.elf")
    if not os.path.exists(elf_path):
        in_loi(f"Không tìm thấy ELF: {elf_path}")
        sys.exit(1)

    # 4. Tìm test cases
    filter_tc = args.tc.split(",") if args.tc else None
    test_cases = tim_test_cases(filter_tc)
    if not test_cases:
        in_loi("Không tìm thấy test case nào!")
        sys.exit(1)

    in_thong_tin(f"Tìm thấy {len(test_cases)} test case")
    print()

    # 5. Chạy từng test case
    renode = RenodeManager(PROJECT_DIR, gdb_port=args.gdb_port)
    gdb = GdbController(elf_path, gdb_port=args.gdb_port)
    ket_qua_list = []
    t_start = time.time()

    for i, tc in enumerate(test_cases):
        print(f"{Mau.XANH_NHAT}── [{i+1}/{len(test_cases)}] {tc.ma}: {tc.ten}{Mau.RESET}")

        # Khởi động Renode
        in_thong_tin("  Khởi động Renode...")
        if not renode.khoi_dong(timeout=20):
            in_loi("  Không thể khởi động Renode!")
            from lib.test_case import KetQuaTestCase
            kq = KetQuaTestCase(
                ma=tc.ma, ten=tc.ten, mo_ta=tc.mo_ta,
                ket_qua=KetQua.LOI, loi="Renode không khởi động được"
            )
            ket_qua_list.append(kq)
            continue

        # Chạy test
        in_thong_tin("  Đang chạy test qua GDB...")
        kq = tc.chay(gdb)
        ket_qua_list.append(kq)

        # Hiển thị kết quả
        if kq.dat:
            in_dat(f"  {kq.ma}: {kq.ten} ({kq.thoi_gian_ms:.0f}ms)")
        else:
            in_khong_dat(f"  {kq.ma}: {kq.ten} ({kq.thoi_gian_ms:.0f}ms)")

        # Chi tiết
        for ct in kq.chi_tiet:
            icon = f"{Mau.XANH}✓{Mau.RESET}" if ct.dat else f"{Mau.DO}✗{Mau.RESET}"
            print(f"    {icon} {ct.ten}: mong đợi={ct.mong_doi}, thực tế={ct.thuc_te}")

        if kq.loi:
            in_loi(f"  {kq.loi}")

        # Verbose: hiển thị GDB output
        if args.verbose and kq.gdb_output:
            print(f"\n{Mau.VANG}── GDB Output ──{Mau.RESET}")
            for line in kq.gdb_output.split("\n")[-20:]:
                print(f"  {line}")
            print(f"{Mau.VANG}─────────────────{Mau.RESET}\n")

        # Dừng Renode
        renode.dung()
        print()

    t_total = time.time() - t_start

    # 6. Tổng kết
    tong = len(ket_qua_list)
    dat = sum(1 for kq in ket_qua_list if kq.dat)
    khong_dat = tong - dat

    print()
    print(f"{Mau.DAM}══════════════════════════════════════════════════{Mau.RESET}")
    print(f"{Mau.DAM}               TỔNG KẾT KẾT QUẢ{Mau.RESET}")
    print(f"{Mau.DAM}══════════════════════════════════════════════════{Mau.RESET}")
    print(f"  Tổng số test    : {tong}")
    print(f"  {Mau.XANH}ĐẠT{Mau.RESET}           : {dat}")
    print(f"  {Mau.DO}KHÔNG ĐẠT{Mau.RESET}     : {khong_dat}")
    print(f"  Thời gian       : {t_total:.1f} giây")
    print(f"{Mau.DAM}══════════════════════════════════════════════════{Mau.RESET}")

    if khong_dat > 0:
        print(f"\n  {Mau.DO}Các test không đạt:{Mau.RESET}")
        for kq in ket_qua_list:
            if not kq.dat:
                print(f"    - {kq.ma}: {kq.ten}")

    # 7. Lưu báo cáo
    results_dir = os.path.join(SCRIPT_DIR, "results")
    os.makedirs(results_dir, exist_ok=True)

    # Báo cáo text
    report_text = tao_bao_cao_text(ket_qua_list, t_total)
    report_txt_path = os.path.join(results_dir, "bao_cao.txt")
    with open(report_txt_path, "w", encoding="utf-8") as f:
        f.write(report_text)

    # Báo cáo JSON
    report_json = tao_bao_cao_json(ket_qua_list, t_total)
    report_json_path = os.path.join(results_dir, "bao_cao.json")
    with open(report_json_path, "w", encoding="utf-8") as f:
        json.dump(report_json, f, ensure_ascii=False, indent=2)

    print(f"\n  Báo cáo đã lưu:")
    print(f"    📄 {report_txt_path}")
    print(f"    📋 {report_json_path}")
    print()

    # Dọn dẹp
    renode.dung()

    sys.exit(khong_dat)


if __name__ == "__main__":
    main()
