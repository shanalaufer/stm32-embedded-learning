# STM32 Embedded Learning

Bare-metal firmware exercises on the STM32 NUCLEO-F446RE, written without HAL — direct register access from the RM0390 reference manual. Part of a roadmap toward an on-device TinyML event-detection platform.

Each subfolder is a self-contained STM32CubeIDE project.

## Exercises
- **LED_Blink** — GPIO output. Blinks the on-board LED (PA5) via direct writes to RCC, MODER, and ODR.
- **GPIO_Input** — GPIO input. Reads the on-board button (PC13) to drive the LED (PA5). PC13 has an external pull-up, so the pin reads low when pressed — the conditional logic is inverted by design, not by mistake.
- **UART_TX** — Serial transmit. Sends bytes over USART2 (PA2) to a host terminal at 115200 baud. Enables clocks across two buses (GPIO on AHB1, USART2 on APB1), configures PA2 in alternate-function mode, derives the baud divisor from the 16 MHz default clock, and polls the TXE status flag before each write.
- **i2c_mpu6050** — I2C master driver for the MPU6050 IMU. Configures I2C1 on PB8/PB9 (AF4, open-drain with pull-ups) for 100 kHz standard mode, then drives the bus by hand: START/STOP/repeated-START conditions, 7-bit addressing with the R/W bit, and byte read/write with ACK/NACK — all by polling SR1 status flags. Wakes the sensor (PWR_MGMT_1), verifies it over WHO_AM_I (lights the LED on `0x68`), and streams accelerometer and gyroscope X/Y/Z over UART. A companion `live_plot.py` reads the serial stream and renders the six axes as rolling matplotlib plots.

## Notes
- All register addresses and bit positions are taken from the RM0390 reference manual and the F446 datasheet.
- Built and flashed with STM32CubeIDE on macOS; serial verified over the ST-LINK Virtual COM Port.