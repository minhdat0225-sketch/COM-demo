#!/usr/bin/env python3
"""
data_verifier.py – Dữ liệu kỳ vọng & hàm kiểm tra cho AUTOSAR COM Stack

Chứa:
  - Expected data cho mỗi I-PDU tại mỗi layer
  - Hàm parse GDB output (hex, decimal, backtrace)
  - Hàm so sánh byte buffer
"""

import re
from typing import Optional, List, Tuple


# ========================================================================
# BẢNG DỮ LIỆU KỲ VỌNG – mỗi I-PDU tại mỗi ranh giới layer
# ========================================================================

# Signal values set by Demo_CAN_EngineCmd() in main.c
ENGINE_CMD = {
    "ten": "EngineCmd",
    "pdu_id_com": 0,
    "can_id": 0x180,
    "dlc": 5,
    "hth": 0,
    "pdu_id_canif": 0,
    "signals": {
        "Throttle": {"value": 75, "byte": 0, "bit_off": 0, "bit_len": 8},
        "Start":    {"value": 1,  "byte": 1, "bit_off": 0, "bit_len": 1},
        "TorqueLimit": {"value": 200, "byte": 2, "bit_off": 0, "bit_len": 8},
        # Alive & CRC are dynamic
    },
    # Expected buffer (byte 0..2 cố định, byte 3-4 dynamic)
    "com_buffer_fixed": {0: 0x4B, 1: 0x01, 2: 0xC8},
}

BRAKE_CMD = {
    "ten": "BrakeCmd",
    "pdu_id_com": 1,
    "can_id": 0x280,
    "dlc": 3,
    "hth": 1,
    "pdu_id_canif": 1,
    "signals": {
        "BrakeReq": {"value": 30, "byte": 0, "bit_off": 0, "bit_len": 8},
        "RegenReq": {"value": 50, "byte": 1, "bit_off": 0, "bit_len": 8},
    },
    "com_buffer_fixed": {0: 0x1E, 1: 0x32},
}

BODY_CMD = {
    "ten": "BodyCmd",
    "pdu_id_com": 2,
    "can_id": 0x380,
    "dlc": 4,
    "hth": 2,
    "pdu_id_canif": 2,
    "signals": {
        "Headlamp": {"value": 1, "byte": 0, "bit_off": 0, "bit_len": 1},
        "TurnL":    {"value": 1, "byte": 0, "bit_off": 1, "bit_len": 1},
        "TurnR":    {"value": 0, "byte": 0, "bit_off": 2, "bit_len": 1},
        "DoorLock": {"value": 1, "byte": 0, "bit_off": 3, "bit_len": 1},
    },
    # byte[0] = bit0=1, bit1=1, bit2=0, bit3=1 → 0b00001011 = 0x0B
    "com_buffer_fixed": {0: 0x0B},
}

LIGHT_CTRL = {
    "ten": "LightCtrl",
    "pdu_id_com": 3,
    "lin_frame_id": 0x10,
    "dlc": 4,
    "linif_pdu_id": 0,
    "signals": {
        "Headlamp":   {"value": 1,  "byte": 0, "bit_off": 0, "bit_len": 1},
        "DRL":        {"value": 1,  "byte": 0, "bit_off": 1, "bit_len": 1},
        "Brightness": {"value": 80, "byte": 1, "bit_off": 0, "bit_len": 8},
    },
    # byte[0] = bit0=1, bit1=1 → 0x03; byte[1] = 80 = 0x50
    "com_buffer_fixed": {0: 0x03, 1: 0x50},
}

HVAC_CTRL = {
    "ten": "HVACCtrl",
    "pdu_id_com": 4,
    "lin_frame_id": 0x11,
    "dlc": 3,
    "linif_pdu_id": 1,
    "signals": {
        "FanSpeed": {"value": 128, "byte": 0, "bit_off": 0, "bit_len": 8},
    },
    "com_buffer_fixed": {0: 0x80},
}

# Routing table: COM PduId → destination
ROUTING_TABLE = {
    0: {"dest": "CANIF", "dst_id": 0},  # EngineCmd
    1: {"dest": "CANIF", "dst_id": 1},  # BrakeCmd
    2: {"dest": "CANIF", "dst_id": 2},  # BodyCmd
    3: {"dest": "LINIF", "dst_id": 0},  # LightCtrl
    4: {"dest": "LINIF", "dst_id": 1},  # HVACCtrl
}

# Thứ tự BSW Init
BSW_INIT_ORDER = [
    "Can_Init",
    "CanIf_Init",
    "LinIf_Init",
    "Lin_Init",
    "PduR_Init",
    "Com_Init",
]


# ========================================================================
# HÀM PARSE GDB OUTPUT
# ========================================================================

def parse_gia_tri_bien(output: str, ten_bien: str) -> Optional[int]:
    """Trích xuất giá trị biến từ printf GDB.

    Tìm pattern: VAR_<ten>=<số> hoặc VAR_HEX_<ten>=0x<hex>
    """
    # Thử hex trước
    m = re.search(rf"VAR_HEX_{ten_bien}\s*=\s*0x([0-9a-fA-F]+)", output)
    if m:
        return int(m.group(1), 16)
    # Thử decimal
    m = re.search(rf"VAR_{ten_bien}\s*=\s*(-?\d+)", output)
    if m:
        return int(m.group(1))
    return None


def parse_memory_bytes(output: str) -> List[int]:
    """Trích xuất byte values từ GDB `x/Nbx` output.

    Ví dụ input:
        0x20000100:  0x4b  0x01  0xc8  0x11  0x00
    Returns:
        [0x4B, 0x01, 0xC8, 0x11, 0x00]
    """
    bytes_found = []
    for line in output.split("\n"):
        # Tìm dòng có format: 0xADDR: 0xVV 0xVV ...
        m = re.findall(r"0x([0-9a-fA-F]{2})\b", line)
        if m and ":" in line:
            # Bỏ qua phần address (trước dấu :)
            colon_idx = line.index(":")
            after_colon = line[colon_idx + 1:]
            values = re.findall(r"0x([0-9a-fA-F]{2})", after_colon)
            bytes_found.extend(int(v, 16) for v in values)
    return bytes_found


def parse_backtrace(output: str) -> List[str]:
    """Trích xuất danh sách tên hàm từ backtrace GDB.

    Ví dụ input:
        #0  Can_Write (Hth=0, ...) at autosar/can/Can.c:247
        #1  0x08001234 in CanIf_Transmit (...)
        #2  PduR_ComTransmit (...)

    Returns:
        ["Can_Write", "CanIf_Transmit", "PduR_ComTransmit"]
    """
    funcs = []
    for line in output.split("\n"):
        # Pattern: #N 0xADDR in FuncName (  hoặc  #N FuncName (
        m = re.search(r"#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?(\w+)\s*\(", line)
        if m:
            funcs.append(m.group(1))
    return funcs


def parse_bp_hits(output: str) -> List[str]:
    """Trích xuất danh sách breakpoint hit từ printf markers.

    Tìm pattern: BP_HIT_N=<func_name>
    """
    hits = []
    for m in re.finditer(r"BP_HIT_\d+=(\w+)", output):
        hits.append(m.group(1))
    return hits


def parse_printf_value(output: str, marker: str) -> Optional[str]:
    """Trích xuất giá trị từ printf với marker cụ thể.

    Ví dụ: marker="CAN_ID" → tìm "CAN_ID=0x180" → return "0x180"
    """
    m = re.search(rf"{marker}=(\S+)", output)
    return m.group(1) if m else None


def kiem_tra_buffer(actual: List[int], expected_fixed: dict) -> List[Tuple[int, int, int, bool]]:
    """So sánh buffer thực tế với expected (chỉ tại các offset cố định).

    Args:
        actual: Danh sách byte thực tế.
        expected_fixed: Dict {byte_index: expected_value}.

    Returns:
        List of (index, expected, actual, match).
    """
    results = []
    for idx, exp_val in expected_fixed.items():
        if idx < len(actual):
            act_val = actual[idx]
            results.append((idx, exp_val, act_val, exp_val == act_val))
        else:
            results.append((idx, exp_val, -1, False))
    return results
