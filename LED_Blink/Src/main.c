#include <stdint.h>

#define RCC_AHB1ENR   (*(volatile uint32_t*)0x40023830)
#define GPIOA_MODER   (*(volatile uint32_t*)0x40020000)
#define GPIOA_ODR     (*(volatile uint32_t*)0x40020014)

int main(void)
{
    // Enable clock to GPIOA
    RCC_AHB1ENR |= (1U << 0);

    // Set PA5 as output (bits 11:10 = 01)
    GPIOA_MODER &= ~(3U << 10);
    GPIOA_MODER |=  (1U << 10);

    for(;;)
    {
        // Turn LED on
        GPIOA_ODR |= (1U << 5);

        // Simple delay
        for(volatile int i = 0; i < 3000000; i++);

        // Turn LED off
        GPIOA_ODR &= ~(1U << 5);

        // Simple delay
        for(volatile int i = 0; i < 3000000; i++);
    }
}
