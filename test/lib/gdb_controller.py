#!/usr/bin/env python3
"""
gdb_controller.py – Điều khiển GDB qua batch mode

Chức năng:
  - Sinh GDB script tạm (.gdb) với các lệnh cần thiết
  - Chạy arm-none-eabi-gdb -batch -x <script> <elf>
  - Parse output text để trích xuất giá trị biến, memory, backtrace
"""

import subprocess
import tempfile
import os
import re
from typing import Optional


class GdbResult:
    """Kết quả sau khi chạy 1 GDB script."""

    def __init__(self, output: str, returncode: int):
        self.output = output
        self.returncode = returncode

    def tim_gia_tri(self, pattern: str) -> Optional[str]:
        """Tìm giá trị khớp regex pattern trong output.

        Returns:
            Chuỗi khớp nhóm đầu tiên, hoặc None.
        """
        m = re.search(pattern, self.output)
        return m.group(1) if m else None

    def tim_tat_ca(self, pattern: str) -> list:
        """Tìm tất cả khớp regex pattern."""
        return re.findall(pattern, self.output)

    def chua_chuoi(self, text: str) -> bool:
        """Kiểm tra output có chứa chuỗi."""
        return text in self.output


class GdbController:
    """Điều khiển arm-none-eabi-gdb qua batch script."""

    def __init__(self, elf_path: str, gdb_port: int = 3333,
                 gdb_cmd: str = "arm-none-eabi-gdb"):
        self.elf_path = os.path.abspath(elf_path)
        self.gdb_port = gdb_port
        self.gdb_cmd = gdb_cmd

    def chay_script(self, gdb_commands: str, timeout: int = 30) -> GdbResult:
        """Chạy một GDB script (batch mode).

        Args:
            gdb_commands: Nội dung GDB script (multi-line string).
            timeout: Thời gian tối đa (giây).

        Returns:
            GdbResult chứa output và return code.
        """
        # Thêm target remote tự động
        full_script = f"target remote :{self.gdb_port}\n{gdb_commands}\nquit\n"

        # Ghi vào file tạm
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".gdb", delete=False
        ) as f:
            f.write(full_script)
            script_path = f.name

        try:
            result = subprocess.run(
                [self.gdb_cmd, "-batch", "-x", script_path, self.elf_path],
                capture_output=True, text=True, timeout=timeout
            )
            output = result.stdout + result.stderr
            return GdbResult(output, result.returncode)
        except subprocess.TimeoutExpired:
            return GdbResult("[LỖI] GDB timeout sau {} giây".format(timeout), -1)
        finally:
            os.unlink(script_path)

    def doc_bien(self, var_name: str, break_at: str,
                 continue_count: int = 1) -> GdbResult:
        """Đặt breakpoint, continue, rồi đọc giá trị biến.

        Args:
            var_name: Tên biến cần đọc.
            break_at: Vị trí breakpoint (hàm hoặc file:line).
            continue_count: Số lần continue trước khi đọc.

        Returns:
            GdbResult – dùng tim_gia_tri() để parse.
        """
        cmds = f"break {break_at}\n"
        for _ in range(continue_count):
            cmds += "continue\n"
        cmds += f'printf "VAR_{var_name}=%d\\n", {var_name}\n'
        cmds += f'printf "VAR_HEX_{var_name}=0x%x\\n", {var_name}\n'
        return self.chay_script(cmds)

    def doc_memory(self, break_at: str, addr_expr: str, count: int,
                   fmt: str = "bx", continue_count: int = 1) -> GdbResult:
        """Đặt breakpoint, continue, rồi đọc vùng nhớ.

        Args:
            break_at: Vị trí breakpoint.
            addr_expr: Biểu thức địa chỉ (ví dụ: "&Com_IpduBuf[0][0]").
            count: Số phần tử cần đọc.
            fmt: Định dạng GDB (bx=byte hex, wx=word hex, ...).
            continue_count: Số lần continue.

        Returns:
            GdbResult – output chứa nội dung memory.
        """
        cmds = f"break {break_at}\n"
        for _ in range(continue_count):
            cmds += "continue\n"
        cmds += f"x/{count}{fmt} {addr_expr}\n"
        return self.chay_script(cmds)

    def kiem_tra_call_chain(self, break_at: str, depth: int = 8,
                            continue_count: int = 1) -> GdbResult:
        """Đặt breakpoint, continue, rồi lấy backtrace.

        Returns:
            GdbResult – output chứa backtrace.
        """
        cmds = f"break {break_at}\n"
        for _ in range(continue_count):
            cmds += "continue\n"
        cmds += f"backtrace {depth}\n"
        return self.chay_script(cmds)

    def chay_nhieu_breakpoint(self, breakpoints: list,
                              reads_at_each: dict = None) -> GdbResult:
        """Đặt nhiều breakpoint, continue qua từng cái, đọc biến tại mỗi điểm.

        Args:
            breakpoints: Danh sách tên hàm/vị trí breakpoint.
            reads_at_each: Dict {bp_name: [list of GDB printf commands]}.

        Returns:
            GdbResult tổng hợp.
        """
        cmds = ""
        for bp in breakpoints:
            cmds += f"break {bp}\n"

        for i, bp in enumerate(breakpoints):
            cmds += "continue\n"
            cmds += f'printf "BP_HIT_{i}={bp}\\n"\n'
            cmds += f"backtrace 3\n"
            if reads_at_each and bp in reads_at_each:
                for read_cmd in reads_at_each[bp]:
                    cmds += read_cmd + "\n"

        return self.chay_script(cmds)
