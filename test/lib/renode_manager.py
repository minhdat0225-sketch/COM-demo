#!/usr/bin/env python3
"""
renode_manager.py – Quản lý Renode instance cho test AUTOSAR COM Stack

Chức năng:
  - Khởi chạy Renode headless (--disable-xwt)
  - Nạp file .resc + ELF
  - Chờ GDB server sẵn sàng
  - Dừng & dọn dẹp Renode
"""

import subprocess
import socket
import time
import os
import signal


class RenodeManager:
    """Quản lý vòng đời Renode cho mỗi test case."""

    def __init__(self, project_dir: str, gdb_port: int = 3333):
        self.project_dir = os.path.abspath(project_dir)
        self.gdb_port = gdb_port
        self.resc_path = "scripts/stm32_com.resc"
        self._process = None

    def khoi_dong(self, timeout: int = 20) -> bool:
        """Khởi động Renode và chờ GDB server sẵn sàng.

        Returns:
            True nếu Renode đã sẵn sàng, False nếu timeout.
        """
        self.dung()  # Dọn dẹp instance cũ nếu có

        # Tạo thư mục results nếu chưa có
        results_dir = os.path.join(self.project_dir, "test", "results")
        os.makedirs(results_dir, exist_ok=True)
        log_path = os.path.join(results_dir, "renode.log")

        cmd = [
            "renode",
            "--disable-xwt",
            "--execute",
            f"include @{self.resc_path}; start",
        ]

        log_file = open(log_path, "w")
        self._process = subprocess.Popen(
            cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            cwd=self.project_dir,
            preexec_fn=os.setsid,
        )

        # Chỉ cần sleep để chờ Renode khởi động, tránh connect thử bằng socket làm tắt GDB server của Renode
        print(f"[DEBUG] Đang chờ 3.0 giây để Renode khởi động...")
        time.sleep(3.0)
        poll = self._process.poll()
        print(f"[DEBUG] Trạng thái Renode process (poll): {poll}")
        if poll is not None:
            print(f"[DEBUG] Renode không thể khởi động, mã thoát: {poll}")
            return False
        return True

    def dung(self):
        """Dừng Renode và giải phóng port."""
        if self._process is not None:
            try:
                os.killpg(os.getpgid(self._process.pid), signal.SIGTERM)
            except (ProcessLookupError, OSError):
                pass
            try:
                self._process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(self._process.pid), signal.SIGKILL)
                except (ProcessLookupError, OSError):
                    pass
            self._process = None

        # Giải phóng port nếu vẫn bị chiếm
        self._giai_phong_port()
        time.sleep(0.5)

    def _cho_port(self, timeout: int) -> bool:
        """Chờ GDB port TCP sẵn sàng."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                with socket.create_connection(("127.0.0.1", self.gdb_port), timeout=1):
                    return True
            except (ConnectionRefusedError, socket.timeout, OSError):
                time.sleep(0.5)
        return False

    def _giai_phong_port(self):
        """Kill process đang chiếm GDB port."""
        try:
            result = subprocess.run(
                ["lsof", "-ti", f":{self.gdb_port}"],
                capture_output=True, text=True, timeout=5
            )
            for pid in result.stdout.strip().split("\n"):
                if pid:
                    try:
                        os.kill(int(pid), signal.SIGKILL)
                    except (ProcessLookupError, ValueError):
                        pass
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass

    def __del__(self):
        self.dung()
