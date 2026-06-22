# STM32 Embedded Learning

Bare-metal firmware exercises on the STM32 NUCLEO-F446RE, written without HAL — direct register access from the RM0390 reference manual. Part of a roadmap toward an on-device TinyML event-detection platform.

Each subfolder is a self-contained STM32CubeIDE project.

## Exercises
- **LED_Blink** — bare-metal GPIO output. Blinks the on-board LED (PA5) using direct writes to RCC, MODER, and ODR.
