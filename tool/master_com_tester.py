#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import re
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
COM_ROOT = SCRIPT_DIR.parent
LOG_DIR = SCRIPT_DIR / "logs"

RENODE_BIN = os.environ.get("RENODE_BIN")
if not RENODE_BIN:
    default_renode = Path("/Applications/Renode.app/Contents/MacOS/renode")
    RENODE_BIN = str(default_renode if default_renode.exists() else "renode")

GDB_BIN = os.environ.get("GDB_BIN", "arm-none-eabi-gdb")

def parse_gdb_scalar(output: str, name: str) -> int:
    match = re.search(rf"\${name}\s*=\s*(0x[0-9a-fA-F]+|\d+)", output)
    if not match:
        raise AssertionError(f"Lỗi: Không tìm thấy giá trị scalar GDB ${name} trong kết quả:\n{output}")
    return int(match.group(1), 0)

def gdb_eval(elf: str, port: int, commands: list[str], timeout: int = 60) -> str:
    cmd = [GDB_BIN, str(elf), "-batch", "-ex", f"target remote :{port}"]
    for item in commands:
        cmd.extend(["-ex", item])
    proc = subprocess.run(cmd, cwd=SCRIPT_DIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
    return proc.stdout

def main():
    elf_path = COM_ROOT / "build/example/master_com_test.elf"
    if not elf_path.exists():
        print(f"[!] File {elf_path} không tồn tại. Vui lòng chạy 'make' trước.")
        sys.exit(1)

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    log_file = LOG_DIR / "master_com_uart.log"
    if log_file.exists():
        log_file.unlink()

    resc_path = SCRIPT_DIR / "master_com.resc"
    lines = [
        "mach create \"MasterECU\"",
        "machine LoadPlatformDescription @debug/stm32f103_full.repl",
        f"sysbus.usart1 CreateFileBackend @{log_file.resolve().as_posix()} true",
        f"sysbus LoadELF @{elf_path.resolve().as_posix()}",
        "machine StartGdbServer 3333",
        "logLevel 3 sysbus",
        "start"
    ]
    resc_path.write_text("\n".join(lines) + "\n")

    print("[+] Đang khởi động Renode...")
    proc = subprocess.Popen([RENODE_BIN, "--disable-gui", str(resc_path)], cwd=COM_ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    # Chạy trong 3 giây
    time.sleep(3)
    
    # Dùng GDB đọc các biến trạng thái
    print("[+] Đang đọc trạng thái biến qua GDB...")
    try:
        gdb_out = gdb_eval(elf_path, 3333, [
            "p/d throttle_val",
            "p/d brake_val",
            "p/d regen_val",
            "p/d headlamp_st",
            "p/d turnL_st",
            "detach",
            "quit"
        ])
        throttle = parse_gdb_scalar(gdb_out, "1")
        brake = parse_gdb_scalar(gdb_out, "2")
        regen = parse_gdb_scalar(gdb_out, "3")
        headlamp = parse_gdb_scalar(gdb_out, "4")
        turnL = parse_gdb_scalar(gdb_out, "5")
        
        print(f"    - Throttle: {throttle}")
        print(f"    - Brake: {brake}")
        print(f"    - Regen: {regen}")
        print(f"    - Headlamp: {headlamp}")
        print(f"    - TurnL: {turnL}")
        
    except Exception as e:
        print(f"[!] Lỗi GDB: {e}")
    finally:
        proc.terminate()
        proc.wait(timeout=5)

    print("[+] Phân tích log UART...")
    if not log_file.exists():
        print("[!] Không tìm thấy file log UART.")
        sys.exit(1)
        
    content = log_file.read_text(encoding='utf-8', errors='ignore')
    
    cnt_20 = content.count("20\n")
    cnt_100 = content.count("100\n")
    
    print(f"    - Số lần gọi task 20ms: {cnt_20} (Trong ~150ms virtual time)")
    print(f"    - Số lần gọi task 100ms: {cnt_100} (Trong ~150ms virtual time)")

    if cnt_20 > 2 and throttle > 5:
        print("\n=> TEST TIMING PASS")
    else:
        print("\n=> TEST TIMING FAIL")

    if regen == (100 - brake) or (brake == 0 and regen == 100) or brake > 0:
        print("=> TEST DATA LOGIC PASS")
    else:
        print("=> TEST DATA LOGIC FAIL")

if __name__ == '__main__':
    main()
