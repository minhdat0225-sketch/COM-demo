#!/bin/bash
##################################################################
# run_all_tests.sh – Chạy toàn bộ test GDB+Renode
##################################################################
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="${PROJECT_DIR}/test"
RESULT_DIR="${TEST_DIR}/results"
ELF="${PROJECT_DIR}/com_demo.elf"
GDB="arm-none-eabi-gdb"
GDB_PORT=3333

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
TOTAL=0; PASSED=0; FAILED=0; FAIL_LIST=""

log_info()  { echo -e "${CYAN}[INFO]${NC} $1"; }
log_pass()  { echo -e "${GREEN}[PASS]${NC} $1"; }
log_fail()  { echo -e "${RED}[FAIL]${NC} $1"; }

kill_renode() {
    pkill -f "renode.*disable-xwt" 2>/dev/null || true
    sleep 2
    lsof -ti :${GDB_PORT} 2>/dev/null | xargs kill -9 2>/dev/null || true
    sleep 1
}

start_renode() {
    kill_renode

    nohup renode --disable-xwt \
        --execute "include @scripts/stm32_com.resc; start" \
        < /dev/null > "${RESULT_DIR}/renode.log" 2>&1 &

    # Dùng Python để chờ port TCP sẵn sàng (tối đa 20s)
    if python3 "${TEST_DIR}/wait_port.py" ${GDB_PORT} 20; then
        sleep 1  # Buffer thêm 1s
        return 0
    else
        log_fail "Renode không khởi động được"
        return 1
    fi
}

run_test() {
    local test_name="$1"
    local gdb_script="$2"
    local pass_pattern="$3"
    local output_file="${RESULT_DIR}/${test_name}.log"

    TOTAL=$((TOTAL + 1))
    local tc_num=$(printf '%02d' $TOTAL)

    start_renode || { FAILED=$((FAILED+1)); FAIL_LIST="${FAIL_LIST}\n  - TC${tc_num}: ${test_name} (Renode fail)"; return; }

    timeout 30 ${GDB} -batch -x "${gdb_script}" "${ELF}" > "${output_file}" 2>&1 || true

    kill_renode

    if grep -q "${pass_pattern}" "${output_file}"; then
        log_pass "TC${tc_num}: ${test_name}"
        PASSED=$((PASSED + 1))
    else
        log_fail "TC${tc_num}: ${test_name}"
        FAILED=$((FAILED + 1))
        FAIL_LIST="${FAIL_LIST}\n  - TC${tc_num}: ${test_name}"
        tail -3 "${output_file}" 2>/dev/null | sed 's/^/       /'
    fi
}

# ===== MAIN =====
cd "${PROJECT_DIR}"

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║  AUTOSAR COM Stack – GDB+Renode Test Suite   ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

mkdir -p "${RESULT_DIR}"

log_info "Build project..."
make clean > /dev/null 2>&1 || true
if make > "${RESULT_DIR}/build.log" 2>&1; then
    log_pass "Build thành công"
else
    log_fail "Build thất bại!"
    exit 1
fi

echo ""
log_info "Chạy 12 test cases..."
echo ""

run_test "bsw_init_sequence"      "${TEST_DIR}/tc01_bsw_init.gdb"       "TC01_PASS"
run_test "can_tx_engine"          "${TEST_DIR}/tc02_can_tx_engine.gdb"   "TC02_PASS"
run_test "can_tx_brake"           "${TEST_DIR}/tc03_can_tx_brake.gdb"    "TC03_PASS"
run_test "can_tx_body"            "${TEST_DIR}/tc04_can_tx_body.gdb"     "TC04_PASS"
run_test "lin_tx_light"           "${TEST_DIR}/tc05_lin_tx_light.gdb"    "TC05_PASS"
run_test "lin_tx_hvac"            "${TEST_DIR}/tc06_lin_tx_hvac.gdb"     "TC06_PASS"
run_test "pdur_routing"           "${TEST_DIR}/tc07_pdur_routing.gdb"    "TC07_PASS"
run_test "signal_packing"         "${TEST_DIR}/tc08_signal_packing.gdb"  "TC08_PASS"
run_test "can_bxcan_regs"         "${TEST_DIR}/tc09_can_bxcan.gdb"      "TC09_PASS"
run_test "lin_usart2"             "${TEST_DIR}/tc10_lin_usart2.gdb"      "TC10_PASS"
run_test "tx_confirmation"        "${TEST_DIR}/tc11_tx_confirm.gdb"      "TC11_PASS"
run_test "continuous_operation"   "${TEST_DIR}/tc12_continuous.gdb"      "TC12_PASS"

kill_renode

echo ""
echo "══════════════════════════════════════════════"
echo "              KẾT QUẢ TỔNG HỢP"
echo "══════════════════════════════════════════════"
echo -e "  Tổng test  : ${TOTAL}"
echo -e "  ${GREEN}PASS${NC}       : ${PASSED}"
echo -e "  ${RED}FAIL${NC}       : ${FAILED}"
if [ ${FAILED} -gt 0 ]; then
    echo -e "\n  Test thất bại:${FAIL_LIST}"
fi
echo "══════════════════════════════════════════════"
exit ${FAILED}
