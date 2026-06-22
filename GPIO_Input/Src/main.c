#include <stdint.h>
#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830)
#define GPIOC_MODER   (*(volatile uint32_t *)0x40020800)
#define GPIOC_IDR     (*(volatile uint32_t *)0x40020810)
#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR     (*(volatile uint32_t *)0x40020014)

int main(void)
{
    RCC_AHB1ENR |= (1U << 0);   // enable GPIOA clock
    RCC_AHB1ENR |= (1U << 2);   // enable GPIOC clock
    GPIOA_MODER |=  (1U << 10);   // PA5 as output (01)
        GPIOC_MODER &= ~(3U << 26);   // PC13 as input  (00)

        while (1)
            {
                if (GPIOC_IDR & (1U << 13))   // button NOT pressed (reads high)
                {
                    GPIOA_ODR &= ~(1U << 5);  // LED off
                }
                else                          // button pressed (reads low)
                {
                    GPIOA_ODR |= (1U << 5);   // LED on
                }
            }
        }
