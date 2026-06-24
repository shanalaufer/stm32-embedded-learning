#include <stdint.h>

#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)  // GPIO clock enable
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)  // USART2 clock enable
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)  // GPIOA pin modes
#define GPIOA_AFRL   (*(volatile uint32_t *)0x40020020)  // GPIOA alternate function low
#define USART2_BRR   (*(volatile uint32_t *)0x40004408)  // baud rate register
#define USART2_CR1   (*(volatile uint32_t *)0x4000440C)  // control register 1
#define USART2_DR    (*(volatile uint32_t *)0x40004404)  // data register
#define USART2_SR    (*(volatile uint32_t *)0x40004400)  // status register

int main(void)
{
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |=  (2U << 4);
    GPIOA_AFRL  &= ~(0xFU << 8);
    GPIOA_AFRL  |=  (7U << 8);
    USART2_BRR = 139;  // 16MHz / 115200 ≈ 139
    USART2_CR1 = (1U << 3) | (1U << 13);  // TE + UE: transmitter on, USART on

    for(;;)
        {
            while (!(USART2_SR & (1U << 7)));  // wait until TXE = 1 (ready)
            USART2_DR = 'H';                   // send the byte
            for (volatile int i = 0; i < 100000; i++);  // small delay so we don't flood
        }
}
