# STM32 Embedded Learning

Bare-metal firmware exercises on the STM32 NUCLEO-F446RE, written without HAL — direct register access from the RM0390 reference manual. Part of a roadmap toward an on-device TinyML event-detection platform.

Each subfolder is a self-contained STM32CubeIDE project.

## Exercises
- **LED_Blink** — GPIO output. Blinks the on-board LED (PA5) via direct writes to RCC, MODER, and ODR.
- **GPIO_Input** — GPIO input. Reads the on-board button (PC13) to drive the LED (PA5). PC13 has an external pull-up, so the pin reads low when pressed — the conditional logic is inverted by design, not by mistake.
- **UART_TX** — Serial transmit. Sends bytes over USART2 (PA2) to a host terminal at 115200 baud. Enables clocks across two buses (GPIO on AHB1, USART2 on APB1), configures PA2 in alternate-function mode, derives the baud divisor from the 16 MHz default clock, and polls the TXE status flag before each write.
- **i2c_mpu6050** — I2C master driver for the MPU6050 IMU, and everything built on top of it. See below.

## i2c_mpu6050 in stages

This project grew in place rather than as separate folders. Each stage builds on the last.

1. **I2C driver.** Configures I2C1 on PB8/PB9 (AF4, open-drain with pull-ups) for 100 kHz standard mode, then drives the bus by hand: START/STOP/repeated-START conditions, 7-bit addressing with the R/W bit, and byte read/write with ACK/NACK — all by polling SR1 status flags. Wakes the sensor (PWR_MGMT_1) and verifies it over WHO_AM_I, lighting the LED on `0x68`.

2. **IMU logging.** Streams accelerometer and gyroscope X/Y/Z over UART. Each axis arrives as two 8-bit registers stitched into a signed 16-bit value. A companion `live_plot.py` reads the serial stream and renders the six axes as rolling matplotlib plots.

3. **Complementary filter.** Combines accelerometer and gyroscope into a single tilt angle. The accelerometer knows which way down is but reacts to every bump; the gyroscope is smooth but its integrated angle drifts. A 98/2 weighted blend gives an angle that is both steady and correct.

4. **Gyro bias calibration.** 500 stationary gyro samples averaged at startup and subtracted from every later reading, since the sensor reports roughly -200 counts at rest. Without it the fused angle sat several degrees low with nothing visibly wrong.

5. **Hardware-timed sampling.** TIM2 prescaled to 1 kHz with ARR 19 produces a 20 ms tick, so samples land at a known 50 Hz rather than at whatever rate a busy-wait delay happens to produce. The loop flags when a tick arrives while the previous iteration is still running, which measures how much headroom the I2C reads and float math actually leave.

6. **FFT.** 128-point radix-2 FFT over the gyro stream. At 50 Hz this gives 0.39 Hz bins up to 25 Hz. The mean is removed before the transform so a DC offset does not dominate bin 0. Prints a text spectrum scaled to the largest bin, plus the peak frequency.

## Notes
- All register addresses and bit positions are taken from the RM0390 reference manual and the F446 datasheet.
- Built and flashed with STM32CubeIDE on macOS; serial verified over the ST-LINK Virtual COM Port.
- The Cortex-M4 FPU is disabled at reset. With `-mfloat-abi=hard` the compiler emits FPU instructions anyway, so the first float operation faults silently — nothing prints, but the init LED is already lit. Enabled via CPACR (`0xE000ED88`), bits 20-23, as the first line of main.
- `i2c_stop()` originally wrote the STOP bit and returned immediately. The peripheral still needs time to drive STOP onto the wires, so with enough transactions per loop the next START began before the bus had settled and the driver hung on the ADDR flag. Fixed by polling the STOP bit until hardware clears it.