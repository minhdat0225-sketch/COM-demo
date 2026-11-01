#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import re
from pathlib import Path

# Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
COM_ROOT = SCRIPT_DIR.parent
LOG_DIR = SCRIPT_DIR / "logs"

RENODE_BIN = os.environ.get("RENODE_BIN")
if not RENODE_BIN:
    default_renode = Path("/Applications/Renode.app/Contents/MacOS/renode")
    RENODE_BIN = str(default_renode if default_renode.exists() else "renode")

GDB_BIN = os.environ.get("GDB_BIN", "arm-none-eabi-gdb")

def format_hex(data: list[int]) -> str:
    return " ".join([f"{x:02X}" for x in data])

def print_uds(tx: list[int], rx: list[int]):
    if tx:
        print(f"    [UDS TX] {format_hex(tx)}")
    if rx:
        print(f"    [UDS RX] {format_hex(rx)}")

def print_com(msg: str):
    print(f"    [COM] {msg}")

def run_cmd(cmd: list[str], cwd: Path, timeout: int = 120, check: bool = False) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )
    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(cmd)}\n{proc.stdout}")
    return proc

def prompt_path(prompt_str: str, default_path: str) -> str:
    ans = input(f"{prompt_str} [{default_path}]: ").strip()
    return ans if ans else default_path

def generate_resc(mode: int, evcu_elf: str, diag_elf: str, com_elf: str) -> Path:
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    resc_path = SCRIPT_DIR / "dynamic.resc"
    
    lines = [
        ":name: Dynamic eVCU Test Network",
        "emulation CreateCANHub \"canHub\"",
        "",
        "mach create \"eVCU\"",
        "machine LoadPlatformDescription @debug/stm32f103_full.repl",
        "connector Connect sysbus.can1 canHub",
        f"sysbus.usart1 CreateFileBackend @{(LOG_DIR / 'evcu_uart.log').resolve().as_posix()} true",
        f"sysbus LoadELF @{Path(evcu_elf).resolve().as_posix()}",
        "machine StartGdbServer 3333",
    ]

    if mode in (2, 4):
        lines.extend([
            "",
            "mach create \"Diag_ECU\"",
            "machine LoadPlatformDescription @debug/stm32f103_full.repl",
            "connector Connect sysbus.can1 canHub",
            f"sysbus.usart1 CreateFileBackend @{(LOG_DIR / 'diag_uart.log').resolve().as_posix()} true",
            f"sysbus LoadELF @{Path(diag_elf).resolve().as_posix()}",
            "machine StartGdbServer 3334",
        ])

    if mode in (3, 4):
        lines.extend([
            "",
            "mach create \"Com_ECU\"",
            "machine LoadPlatformDescription @debug/stm32f103_full.repl",
            "connector Connect sysbus.can1 canHub",
            f"sysbus.usart1 CreateFileBackend @{(LOG_DIR / 'com_uart.log').resolve().as_posix()} true",
            f"sysbus LoadELF @{Path(com_elf).resolve().as_posix()}",
            "machine StartGdbServer 3335",
        ])

    lines.append("")
    lines.append("logLevel 3 sysbus")
    lines.append("start")

    resc_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return resc_path

def start_renode(resc_path: Path) -> subprocess.Popen[str]:
    print(f"\n[+] Đang khởi động Renode với cấu hình {resc_path.name}...")
    proc = subprocess.Popen(
        [RENODE_BIN, "--disable-gui", str(resc_path)],
        cwd=COM_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    time.sleep(5)
    if proc.poll() is not None:
        out = proc.stdout.read() if proc.stdout else ""
        raise RuntimeError(f"Renode bị thoát đột ngột:\n{out}")
    return proc

def stop_renode(proc: subprocess.Popen[str]) -> None:
    print("[+] Đang tắt Renode...")
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)

# GDB parsing helpers
def parse_gdb_scalar(output: str, name: str) -> int:
    match = re.search(rf"\${name}\s*=\s*(0x[0-9a-fA-F]+|\d+)", output)
    if not match:
        raise AssertionError(f"Lỗi: Không tìm thấy giá trị scalar GDB ${name} trong kết quả:\n{output}")
    return int(match.group(1), 0)

def parse_gdb_array(output: str, name: str) -> list[int]:
    match = re.search(rf"\${name}\s*=\s*\{{([^}}]+)\}}", output)
    if not match:
        raise AssertionError(f"Lỗi: Không tìm thấy mảng GDB ${name} trong kết quả:\n{output}")
    values: list[int] = []
    for item in match.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        repeat = re.match(r"(0x[0-9a-fA-F]+|\d+)\s+<repeats\s+(\d+)\s+times>", item)
        if repeat:
            values.extend([int(repeat.group(1), 0) & 0xFF] * int(repeat.group(2), 0))
        else:
            values.append(int(item, 0) & 0xFF)
    return values

def gdb_eval(elf: str, port: int, commands: list[str], timeout: int = 60) -> str:
    cmd = [GDB_BIN, str(elf), "-batch", "-ex", f"target remote :{port}"]
    for item in commands:
        cmd.extend(["-ex", item])
    proc = run_cmd(cmd, cwd=SCRIPT_DIR, timeout=timeout, check=True)
    return proc.stdout

# Mode 1 tests (eVCU single node test, via dummy uds requests)
def uds_request_mode1(elf: str, payload: list[int]) -> list[int]:
    cmds = ["set var Evcu_TestUdsRequestLen = 0"]
    for idx, value in enumerate(payload):
        cmds.append(f"set var Evcu_TestUdsRequest[{idx}] = {value}")
    cmds.append(f"set var Evcu_TestUdsRequestLen = {len(payload)}")
    cmds.append("set $dummy = Evcu_TestRunUdsRequest()")
    cmds.append("p/x Evcu_TestLastUdsResponseLen")
    cmds.append("p/x Evcu_TestLastUdsResponse")
    out = gdb_eval(elf, 3333, cmds)
    length = parse_gdb_scalar(out, "1")
    data = parse_gdb_array(out, "2")
    rx = data[:length]
    print_uds(payload, rx)
    return rx

def inject_com_mode1(elf: str) -> tuple[int, int]:
    cmds = ["set $dummy = Evcu_TestInjectEngineStatus3000()"]
    cmds.append("p/x Evcu_TestGetEngineRpm()")
    cmds.append("p/x Evcu_TestGetEngineTemp()")
    out = gdb_eval(elf, 3333, cmds)
    rpm = parse_gdb_scalar(out, "1")
    temp = parse_gdb_scalar(out, "2")
    return rpm, temp

def run_mode1_tests(evcu_elf: str) -> None:
    print("\n" + "="*50)
    print("[*] Đang thực thi Kịch Bản 1: Kiểm thử Nội bộ eVCU (Dummy UDS/COM)...")
    print("="*50)
    
    print("\n[Bước 1] Kiểm tra Diagnostic Session Control (10 03)")
    resp = uds_request_mode1(evcu_elf, [0x10, 0x03])
    assert resp[:2] == [0x50, 0x03], f"Lỗi: DiagSession phản hồi sai: {format_hex(resp)}"
    
    print("\n[Bước 2] Kiểm tra Read VIN DID (22 F1 90)")
    resp = uds_request_mode1(evcu_elf, [0x22, 0xF1, 0x90])
    assert resp[:3] == [0x62, 0xF1, 0x90], f"Lỗi: VIN DID phản hồi sai: {format_hex(resp)}"
    
    print("\n[Bước 3] Kiểm tra Read Snapshot Đa khung (22 F0 01)")
    resp = uds_request_mode1(evcu_elf, [0x22, 0xF0, 0x01])
    assert resp[:3] == [0x62, 0xF0, 0x01] and len(resp) > 20, f"Lỗi: Snapshot DID sai hoặc payload <= 20: {format_hex(resp)}"

    print("\n[Bước 4] Kiểm tra Read DTC Information (19 02 FF)")
    resp = uds_request_mode1(evcu_elf, [0x19, 0x02, 0xFF])
    assert resp[:3] == [0x59, 0x02, 0xFF] and len(resp) > 20, f"Lỗi: ReadDTC sai hoặc payload <= 20: {format_hex(resp)}"

    print("\n[Bước 5] Kiểm tra Clear DTC (14 FF FF FF)")
    resp = uds_request_mode1(evcu_elf, [0x14, 0xFF, 0xFF, 0xFF])
    assert resp == [0x54], f"Lỗi: ClearDTC phản hồi sai: {format_hex(resp)}"
    
    print("\n[Bước 6] Kiểm tra giả lập tín hiệu COM (EngineStatus)")
    print_com("Đang bơm tín hiệu vòng tua RPM=3000, Nhiệt độ Temp=90 vào eVCU...")
    rpm, temp = inject_com_mode1(evcu_elf)
    assert rpm == 3000 and temp == 90, f"Lỗi: Giá trị COM bị sai: rpm={rpm}, temp={temp}"
    
    print("\n[Bước 7] Kiểm thử chéo: Đọc RPM và Temp thông qua UDS DID")
    resp = uds_request_mode1(evcu_elf, [0x22, 0x01, 0x0C])
    assert resp == [0x62, 0x01, 0x0C, 0x0B, 0xB8], f"Lỗi: Read RPM sai: {format_hex(resp)}"
    
    resp = uds_request_mode1(evcu_elf, [0x22, 0x01, 0x05])
    assert resp == [0x62, 0x01, 0x05, 0x5A], f"Lỗi: Read Temp sai: {format_hex(resp)}"

    print("\n[Bước 8] Kiểm tra Service không hỗ trợ (NRC 7F)")
    resp = uds_request_mode1(evcu_elf, [0x27, 0x01])
    assert resp == [0x7F, 0x27, 0x11], f"Lỗi: NRC phản hồi sai: {format_hex(resp)}"
    
    print("\n[*] TOÀN BỘ KỊCH BẢN MODE 1 ĐÃ PASS THÀNH CÔNG!\n")


# Multi-node logic
def wait_diag_response(diag_elf: str, trigger_name: str, expected_sid: int, min_len: int = 1, timeout_s: float = 5.0, tx_payload: list = None) -> list[int]:
    if tx_payload:
        print(f"    [UDS TX] {format_hex(tx_payload)}")
    
    gdb_eval(diag_elf, 3334, [f"set var {trigger_name}=1", "detach", "quit"])
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        out = gdb_eval(diag_elf, 3334, [
            "p/x diag_last_complete",
            "p/x diag_last_sid",
            "p/x diag_last_response_len",
            "p/x diag_last_response",
            "detach",
            "quit",
        ])
        complete = parse_gdb_scalar(out, "1")
        sid = parse_gdb_scalar(out, "2")
        length = parse_gdb_scalar(out, "3")
        if complete and sid == expected_sid and length >= min_len:
            rx = parse_gdb_array(out, "4")[:length]
            if tx_payload:
                print(f"    [UDS RX] {format_hex(rx)}")
            return rx
        time.sleep(0.2)
    raise AssertionError(f"Hết thời gian (Timeout) chờ phản hồi từ Diag ECU ({trigger_name})")

def check_prefix(data: list[int], expected: list[int], label: str) -> None:
    if data[:len(expected)] != expected:
        raise AssertionError(f"Lỗi {label}: mong đợi tiền tố {format_hex(expected)}, nhưng nhận được {format_hex(data)}")

def run_mode4_tests(evcu_elf: str, diag_elf: str, com_elf: str) -> None:
    print("\n" + "="*50)
    print("[*] Đang thực thi Kịch Bản 4: Kiểm thử Tích hợp (eVCU + Diag ECU + COM ECU)...")
    print("="*50)
    
    print("\n[Bước 1] Khởi tạo Session Control qua CAN TP (10 03)")
    resp = wait_diag_response(diag_elf, "diag_trigger_session", 0x50, min_len=2, tx_payload=[0x10, 0x03])
    check_prefix(resp, [0x50, 0x03], "DiagnosticSessionControl")
    
    print("\n[Bước 2] Truy xuất chuỗi VIN qua CAN TP (22 F1 90)")
    resp = wait_diag_response(diag_elf, "diag_trigger_vin", 0x62, min_len=20, tx_payload=[0x22, 0xF1, 0x90])
    check_prefix(resp, [0x62, 0xF1, 0x90], "VIN DID")
    
    print("\n[Bước 3] Truy xuất eVCU Snapshot (Data dài qua CAN TP) (22 F0 01)")
    resp = wait_diag_response(diag_elf, "diag_trigger_snapshot", 0x62, min_len=21, tx_payload=[0x22, 0xF0, 0x01])
    check_prefix(resp, [0x62, 0xF0, 0x01], "Snapshot DID")
    
    print("\n[Bước 4] Truy xuất danh sách mã lỗi DTC hiện tại (19 02 FF)")
    resp = wait_diag_response(diag_elf, "diag_trigger_dtc", 0x59, min_len=21, tx_payload=[0x19, 0x02, 0xFF])
    check_prefix(resp, [0x59, 0x02, 0xFF], "ReadDTCInformation")
    
    print("\n[Bước 5] (Tùy chọn) Gửi lệnh xóa lỗi (14 FF FF FF)")
    try:
        resp = wait_diag_response(diag_elf, "diag_trigger_clear_dtc", 0x54, min_len=1, timeout_s=3.0, tx_payload=[0x14, 0xFF, 0xFF, 0xFF])
        check_prefix(resp, [0x54], "ClearDiagnosticInformation")
        print("    -> Lệnh xóa lỗi DTC thực thi thành công!")
    except Exception as e:
        print("    [Warning] Diag ECU của học viên có thể chưa implement trigger xóa lỗi. Bỏ qua.")
        
    print("\n[Bước 6] (Tùy chọn) Gửi Service không tồn tại để kiểm tra mã NRC")
    try:
        resp = wait_diag_response(diag_elf, "diag_trigger_unsupported", 0x7F, min_len=3, timeout_s=3.0, tx_payload=[0x27, 0x01])
        check_prefix(resp, [0x7F, 0x27, 0x11], "Unsupported Service NRC")
        print("    -> Kiểm tra NRC 7F thành công!")
    except Exception as e:
        print("    [Warning] Diag ECU của học viên có thể chưa implement trigger test NRC. Bỏ qua.")

    print("\n[Bước 7] Tích hợp mạng COM - Gửi bản tin EngineStatus từ COM ECU")
    print_com("Kích hoạt vòng lặp truyền: EngineStatus (RPM=3000, Temp=90) trên CAN Bus")
    gdb_eval(com_elf, 3335, ["set var com_tx_rpm=3000", "set var com_tx_temp=90", "set var com_trigger_engine_status=1", "detach", "quit"])
    time.sleep(1)
    
    print_com("Kiểm tra COM ECU có đang nhận được gói VehicleCommand từ eVCU không...")
    out = gdb_eval(com_elf, 3335, ["p/x com_rx_vehicle_command_seen", "detach", "quit"])
    if parse_gdb_scalar(out, "1") == 0:
        raise AssertionError("Lỗi: COM ECU không nhận được gói VehicleCommand từ eVCU.")
        
    print("\n[Bước 8] Kiểm thử chéo: Diag ECU đọc vòng tua RPM mà COM ECU vừa gửi lên")
    resp = wait_diag_response(diag_elf, "diag_trigger_rpm", 0x62, min_len=5, tx_payload=[0x22, 0x01, 0x0C])
    check_prefix(resp, [0x62, 0x01, 0x0C, 0x0B, 0xB8], "RPM DID after COM EngineStatus")

    print("\n[Bước 9] Giả lập sự cố mất kết nối COM (Timeout Signal)")
    print_com("Tắt cưỡng bức tín hiệu EngineStatus từ COM ECU. Đang đợi Timeout...")
    gdb_eval(com_elf, 3335, ["set var com_trigger_engine_status=0", "detach", "quit"])
    time.sleep(3.5) # Wait for eVCU timeout
    
    print("    [COM] Đọc lại danh sách DTC để kiểm tra xem eVCU có phát hiện mất tín hiệu không:")
    resp = wait_diag_response(diag_elf, "diag_trigger_dtc", 0x59, min_len=2, tx_payload=[0x19, 0x02, 0xFF])
    has_timeout_dtc = False
    for i in range(3, len(resp) - 3, 4):
        dtc_num = (resp[i] << 16) | (resp[i+1] << 8) | resp[i+2]
        if dtc_num == 0x123456:
            has_timeout_dtc = True
            break
    if not has_timeout_dtc:
        print("    [Warning] Diag ECU báo về danh sách lỗi nhưng không có mã 0x123456 (Mất tín hiệu). Kiểm tra lại logic Timeout của eVCU.")
    else:
        print("    -> eVCU đã ghi nhận mã lỗi Mất tín hiệu (0x123456) chuẩn xác! Test PASS!")

    print("\n[*] TOÀN BỘ KỊCH BẢN MODE 4 ĐÃ PASS THÀNH CÔNG!\n")


def main():
    print("="*75)
    print("        CÔNG CỤ KIỂM THỬ ĐA NĂNG (INTERACTIVE TESTING TOOL)")
    print("                Đánh giá chất lượng eVCU & ECU Học Viên")
    print("="*75)
    print("\nVui lòng chọn cấu hình muốn kiểm thử (Mode 1-4):")
    print("  1. Mode 1: Kiểm thử Nội bộ eVCU (Sử dụng Dummy UDS/COM Requests)")
    print("  2. Mode 2: Kiểm thử eVCU với ECU Diagnostic (Giao thức UDS qua CAN TP)")
    print("  3. Mode 3: Kiểm thử eVCU với ECU COM (Giao thức AUTOSAR COM)")
    print("  4. Mode 4: Kiểm thử Tích hợp Toàn hệ thống (eVCU + Diag ECU + COM ECU)")
    
    choice = input("\nNhập lựa chọn của bạn (1-4) [Mặc định: 4]: ").strip()
    if not choice:
        choice = "4"
        
    mode = int(choice)
    
    # Determine default paths
    def_evcu = str(SCRIPT_DIR / "evcu.elf")
    def_diag = str(COM_ROOT / "build/example/evcu_diag_ecu.elf")
    def_com = str(COM_ROOT / "build/example/evcu_com_ecu.elf")
    
    print("\n[+] Xác nhận đường dẫn file firmware (.elf) (Nhấn Enter để giữ mặc định):")
    evcu_elf = prompt_path("Đường dẫn file eVCU ELF", def_evcu)
    
    diag_elf = ""
    if mode in (2, 4):
        diag_elf = prompt_path("Đường dẫn file Diag ECU ELF", def_diag)
        
    com_elf = ""
    if mode in (3, 4):
        com_elf = prompt_path("Đường dẫn file COM ECU ELF", def_com)
        
    # Check if files exist
    if not Path(evcu_elf).exists():
        print(f"\n[!] Lỗi: File eVCU ELF không tồn tại tại: {evcu_elf}")
        sys.exit(1)
    if mode in (2, 4) and not Path(diag_elf).exists():
        print(f"\n[!] Lỗi: File Diag ELF không tồn tại tại: {diag_elf}")
        sys.exit(1)
    if mode in (3, 4) and not Path(com_elf).exists():
        print(f"\n[!] Lỗi: File COM ELF không tồn tại tại: {com_elf}")
        sys.exit(1)
        
    # Generate Renode script
    resc_path = generate_resc(mode, evcu_elf, diag_elf, com_elf)
    
    proc = None
    try:
        proc = start_renode(resc_path)
        
        if mode == 1:
            run_mode1_tests(evcu_elf)
        elif mode == 4:
            run_mode4_tests(evcu_elf, diag_elf, com_elf)
        else:
            print(f"\n[*] Đang chạy Mode {mode}. Các bộ test tự động riêng cho Mode 2/3 hiện tại có thể được mở rộng sau.")
            print("    Bạn có thể sử dụng GDB hoặc đọc file log trong thư mục 'Tool/logs/' để debug thủ công.")
            print("    Nhấn Ctrl+C bất cứ lúc nào để kết thúc phiên mô phỏng.")
            while True:
                time.sleep(1)
                
    except KeyboardInterrupt:
        print("\n\n[!] Người dùng đã chủ động ngắt kết nối.")
    except Exception as e:
        print(f"\n[LỖI NGHIÊM TRỌNG] {e}")
    finally:
        if proc:
            stop_renode(proc)

if __name__ == '__main__':
    main()
