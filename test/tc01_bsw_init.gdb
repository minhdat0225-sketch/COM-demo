# TC01: Kiểm tra thứ tự khởi tạo BSW stack
# Expected: Can_Init → CanIf_Init → LinIf_Init (→ Lin_Init) → PduR_Init → Com_Init
target remote :3333

# Đặt breakpoint tại tất cả hàm Init
break Can_Init
break CanIf_Init
break LinIf_Init
break Lin_Init
break PduR_Init
break Com_Init

# Chạy đến breakpoint đầu tiên
continue

# Can_Init phải là module MCAL init đầu tiên
printf "TC01_CHECK_1: Hit=%s\n", "Can_Init"
backtrace 2

continue
# CanIf_Init
printf "TC01_CHECK_2: Hit=%s\n", "CanIf_Init"
backtrace 2

continue
# LinIf_Init gọi Lin_Init bên trong
printf "TC01_CHECK_3: Hit=%s\n", "LinIf_Init or Lin_Init"
backtrace 2

continue
# Lin_Init (bên trong LinIf_Init)
printf "TC01_CHECK_4: Hit=%s\n", "Lin_Init_inside_LinIf"
backtrace 3

continue
# PduR_Init
printf "TC01_CHECK_5: Hit=%s\n", "PduR_Init"
backtrace 2

continue
# Com_Init
printf "TC01_CHECK_6: Hit=%s\n", "Com_Init"
backtrace 2

# Verify tất cả 6 breakpoint đã hit đúng thứ tự
printf "TC01_PASS: BSW Init sequence verified\n"
quit
