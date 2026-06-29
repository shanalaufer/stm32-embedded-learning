#include <stdint.h>
/* ---------- Base addresses (from RM0390 memory map) ---------- */
#define PERIPH_BASE     (0x40000000U)

#define RCC_BASE        (0x40023800U)
#define GPIOB_BASE      (0x40020400U)
#define I2C1_BASE       (0x40005400U)
#define MPU6050_ADDR        0x68U   /* I2C bus address of the sensor   */
#define MPU6050_WHOAMI      0x75U   /* internal register to read       */
#define GPIOA_BASE      (0x40020000U)
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

/* ---------- RCC registers ---------- */
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* ---------- GPIOB registers ---------- */
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))
#define GPIOB_AFRH      (*(volatile uint32_t *)(GPIOB_BASE + 0x24))

/* ---------- I2C1 registers ---------- */
#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_CCR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_TRISE      (*(volatile uint32_t *)(I2C1_BASE + 0x20))
#define I2C1_DR         (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_SR1        (*(volatile uint32_t *)(I2C1_BASE + 0x14))
#define I2C1_SR2        (*(volatile uint32_t *)(I2C1_BASE + 0x18))

/* ---------- I2C helper functions ---------- */

/* Wait until a status flag in SR1 is set (polling) */
static void i2c_wait_flag_sr1(uint32_t flag)
{
    while (!(I2C1_SR1 & flag));
}

/* Generate START condition */
static void i2c_start(void)
{
    I2C1_CR1 |= (1U << 8);              /* START bit */
    i2c_wait_flag_sr1(1U << 0);         /* wait for SB (start bit) flag */
}

/* Generate STOP condition */
static void i2c_stop(void)
{
    I2C1_CR1 |= (1U << 9);              /* STOP bit */
}

/* Send the 7-bit device address with R/W bit */
static void i2c_send_address(uint8_t addr, uint8_t read)
{
    I2C1_DR = (addr << 1) | (read & 1U);   /* shift addr, append R/W bit */
    i2c_wait_flag_sr1(1U << 1);            /* wait for ADDR flag         */
    (void)I2C1_SR2;                        /* clear ADDR by reading SR2  */
}

/* Write one data byte */
static void i2c_write_byte(uint8_t data)
{
    i2c_wait_flag_sr1(1U << 7);   /* wait for TXE (transmit empty) */
    I2C1_DR = data;
    i2c_wait_flag_sr1(1U << 2);   /* wait for BTF (byte transfer finished) */
}

/* Read one byte with ACK or NACK afterward */
static uint8_t i2c_read_byte(uint8_t ack)
{
    if (ack)
        I2C1_CR1 |= (1U << 10);    /* ACK: ask for another byte */
    else
        I2C1_CR1 &= ~(1U << 10);   /* NACK: this is the last byte */

    i2c_wait_flag_sr1(1U << 6);    /* wait for RXNE (receive not empty) */
    return (uint8_t)I2C1_DR;
}

/* Read one register from the MPU6050 */
static uint8_t mpu6050_read_reg(uint8_t reg)
{
    i2c_start();
    i2c_send_address(MPU6050_ADDR, 0);   /* 0 = write mode */
    i2c_write_byte(reg);                 /* say which register we want */

    i2c_start();                         /* repeated START */
    i2c_send_address(MPU6050_ADDR, 1);   /* 1 = read mode  */
    uint8_t value = i2c_read_byte(0);    /* read 1 byte, NACK it */
    i2c_stop();

    return value;
}

int main(void)
{
    /* ---------- 1. Enable clocks ---------- */
    RCC_AHB1ENR |= (1U << 1);   /* GPIOB clock on (bit 1 = GPIOBEN) */
    RCC_APB1ENR |= (1U << 21);  /* I2C1 clock on  (bit 21 = I2C1EN) */

    /* ---------- 2. Configure PB8 (SCL) and PB9 (SDA) for I2C ---------- */

        /* MODER: set PB8 and PB9 to Alternate Function mode (0b10) */
        GPIOB_MODER &= ~((3U << 16) | (3U << 18));  /* clear both pins' bits */
        GPIOB_MODER |=  ((2U << 16) | (2U << 18));  /* set both to AF mode   */

        /* OTYPER: set PB8 and PB9 to open-drain (1) */
        GPIOB_OTYPER |= (1U << 8) | (1U << 9);

        /* PUPDR: enable pull-ups on PB8 and PB9 (0b01) */
        GPIOB_PUPDR &= ~((3U << 16) | (3U << 18));  /* clear */
        GPIOB_PUPDR |=  ((1U << 16) | (1U << 18));  /* pull-up */

        /* AFRH: select AF4 (I2C1) for PB8 and PB9 */
        GPIOB_AFRH &= ~((0xFU << 0) | (0xFU << 4));  /* clear PB8, PB9 nibbles */
        GPIOB_AFRH |=  ((4U << 0) | (4U << 4));      /* AF4 for both           */

        /* ---------- 3. Configure I2C1 ---------- */

            //I2C1_CR1 |= (1U << 15);   /* SWRST: software reset (clear peripheral) */
          // I2C1_CR1 &= ~(1U << 15);  /* release reset                            */

            I2C1_CR2 |= (16U << 0);   /* FREQ = 16 MHz (APB1 clock in MHz)        */

            I2C1_CCR |= (80U << 0);   /* CCR = 80 -> 100 kHz standard mode        */

            I2C1_TRISE = 17U;         /* max rise time for standard mode          */

            I2C1_CR1 |= (1U << 0);    /* PE: enable the I2C1 peripheral

                    */

            /* Enable GPIOA clock for the onboard LED (PA5) */
                RCC_AHB1ENR |= (1U << 0);          /* GPIOA clock on */
                GPIOA_MODER &= ~(3U << 10);        /* clear PA5 mode bits */
                GPIOA_MODER |=  (1U << 10);        /* PA5 = output */

                /* Read WHO_AM_I and light the LED if it matches */
                uint8_t who = mpu6050_read_reg(MPU6050_WHOAMI);

                if (who == 0x68U) {
                    GPIOA_ODR |= (1U << 5);        /* LED ON  = success */
                } else {
                    GPIOA_ODR &= ~(1U << 5);       /* LED OFF = failure */
                }

    /* Loop forever */
    for(;;);
}
