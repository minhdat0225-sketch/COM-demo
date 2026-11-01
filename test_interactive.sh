#!/bin/bash
> tx_uart.log
> rx_uart.log
renode --disable-gui scripts/stm32_interactive_cantp.resc > /dev/null 2>&1 &
REPID=$!
sleep 5
arm-none-eabi-gdb node_tx.elf --batch -ex "target remote :3333" -ex "set trigger_tx=1" -ex "detach" -ex "quit"
sleep 5
echo "========== RX UART LOG =========="
cat rx_uart.log
echo "========== TX UART LOG =========="
cat tx_uart.log
kill $REPID
