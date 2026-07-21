#include <stdint.h>
#include <stdio.h>
#include <math.h>
/* ---------- Base addresses (from RM0390 memory map) ---------- */
#define PERIPH_BASE     (0x40000000U)

#define CPACR   (*(volatile uint32_t *)(0xE000ED88U))

#define RCC_BASE        (0x40023800U)
#define GPIOB_BASE      (0x40020400U)
#define I2C1_BASE       (0x40005400U)
#define MPU6050_ADDR        0x68U   /* I2C bus address of the sensor   */
#define MPU6050_WHOAMI      0x75U   /* internal register to read       */
#define GPIOA_BASE      (0x40020000U)
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

#define USART2_BASE     (0x40004400U)
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0C))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

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

/* ---------- TIM2 registers ---------- */
#define TIM2_BASE       (0x40000000U)
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_SR         (*(volatile uint32_t *)(TIM2_BASE + 0x10))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2C))

#define ACCEL_SCALE   16384.0f   /* counts per g at +/-2g */
#define GYRO_SCALE    131.0f     /* counts per deg/s at +/-250 dps */
#define ALPHA         0.98f      /* filter weight: gyro vs accel */

/* ---------- I2C helper functions ---------- */

/* Wait until a status flag in SR1 is set (polling) */
static void i2c_wait_flag_sr1(uint32_t flag) {
	while (!(I2C1_SR1 & flag))
		;
}

/* Generate START condition */
static void i2c_start(void) {
	I2C1_CR1 |= (1U << 8); /* START bit */
	i2c_wait_flag_sr1(1U << 0); /* wait for SB (start bit) flag */
}

/* Generate STOP condition */
static void i2c_stop(void) {
	I2C1_CR1 |= (1U << 9);              /* STOP bit */
	while (I2C1_CR1 & (1U << 9));       /* wait until STOP is cleared by hardware */
}

/* Send the 7-bit device address with R/W bit */
static void i2c_send_address(uint8_t addr, uint8_t read) {
	I2C1_DR = (addr << 1) | (read & 1U); /* shift addr, append R/W bit */
	i2c_wait_flag_sr1(1U << 1); /* wait for ADDR flag         */
	(void) I2C1_SR2; /* clear ADDR by reading SR2  */
}

/* Write one data byte */
static void i2c_write_byte(uint8_t data) {
	i2c_wait_flag_sr1(1U << 7); /* wait for TXE (transmit empty) */
	I2C1_DR = data;
	i2c_wait_flag_sr1(1U << 7); /* wait for TXE again (byte accepted) */
}

/* Read one byte with ACK or NACK afterward */
static uint8_t i2c_read_byte(uint8_t ack) {
	if (ack)
		I2C1_CR1 |= (1U << 10); /* ACK: ask for another byte */
	else
		I2C1_CR1 &= ~(1U << 10); /* NACK: this is the last byte */

	i2c_wait_flag_sr1(1U << 6); /* wait for RXNE (receive not empty) */
	return (uint8_t) I2C1_DR;
}

/* Write one register on the MPU6050 */
static void mpu6050_write_reg(uint8_t reg, uint8_t value) {
	i2c_start();
	i2c_send_address(MPU6050_ADDR, 0); /* 0 = write mode */
	i2c_write_byte(reg); /* which register */
	i2c_write_byte(value); /* value to put in it */
	i2c_stop();
}

/* Read one register from the MPU6050 */
static uint8_t mpu6050_read_reg(uint8_t reg) {
	i2c_start();
	i2c_send_address(MPU6050_ADDR, 0); /* 0 = write mode */
	i2c_write_byte(reg); /* say which register we want */

	i2c_start(); /* repeated START */
	i2c_send_address(MPU6050_ADDR, 1); /* 1 = read mode  */
	uint8_t value = i2c_read_byte(0); /* read 1 byte, NACK it */
	i2c_stop();

	return value;
}

/* Read accelerometer X, Y, Z (each a signed 16-bit value) */
static void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
	uint8_t xh = mpu6050_read_reg(0x3B); /* ACCEL_XOUT_H */
	uint8_t xl = mpu6050_read_reg(0x3C); /* ACCEL_XOUT_L */
	uint8_t yh = mpu6050_read_reg(0x3D);
	uint8_t yl = mpu6050_read_reg(0x3E);
	uint8_t zh = mpu6050_read_reg(0x3F);
	uint8_t zl = mpu6050_read_reg(0x40);

	*ax = (int16_t) ((xh << 8) | xl); /* combine high + low into 16-bit */
	*ay = (int16_t) ((yh << 8) | yl);
	*az = (int16_t) ((zh << 8) | zl);
}

/* Read gyroscope X, Y, Z (each a signed 16-bit value) */
static void mpu6050_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz) {
	uint8_t xh = mpu6050_read_reg(0x43); /* GYRO_XOUT_H */
	uint8_t xl = mpu6050_read_reg(0x44); /* GYRO_XOUT_L */
	uint8_t yh = mpu6050_read_reg(0x45);
	uint8_t yl = mpu6050_read_reg(0x46);
	uint8_t zh = mpu6050_read_reg(0x47);
	uint8_t zl = mpu6050_read_reg(0x48);

	*gx = (int16_t) ((xh << 8) | xl);
	*gy = (int16_t) ((yh << 8) | yl);
	*gz = (int16_t) ((zh << 8) | zl);
}

/* Send one character over UART */
static void uart_send_char(char c) {
	while (!(USART2_SR & (1U << 7)))
		; /* wait for TXE (transmit empty) */
	USART2_DR = c;
}

/* Send a null-terminated string */
static void uart_send_string(const char *s) {
	while (*s)
		uart_send_char(*s++);
}

int main(void) {

	CPACR |= (0xFU << 20);   /* enable FPU: full access to CP10 and CP11 */

	/* ---------- 1. Enable clocks ---------- */
	RCC_AHB1ENR |= (1U << 1); /* GPIOB clock on (bit 1 = GPIOBEN) */
	RCC_APB1ENR |= (1U << 21); /* I2C1 clock on  (bit 21 = I2C1EN) */

	/* ---------- 2. Configure PB8 (SCL) and PB9 (SDA) for I2C ---------- */

	/* MODER: set PB8 and PB9 to Alternate Function mode (0b10) */
	GPIOB_MODER &= ~((3U << 16) | (3U << 18)); /* clear both pins' bits */
	GPIOB_MODER |= ((2U << 16) | (2U << 18)); /* set both to AF mode   */

	/* OTYPER: set PB8 and PB9 to open-drain (1) */
	GPIOB_OTYPER |= (1U << 8) | (1U << 9);

	/* PUPDR: enable pull-ups on PB8 and PB9 (0b01) */
	GPIOB_PUPDR &= ~((3U << 16) | (3U << 18)); /* clear */
	GPIOB_PUPDR |= ((1U << 16) | (1U << 18)); /* pull-up */

	/* AFRH: select AF4 (I2C1) for PB8 and PB9 */
	GPIOB_AFRH &= ~((0xFU << 0) | (0xFU << 4)); /* clear PB8, PB9 nibbles */
	GPIOB_AFRH |= ((4U << 0) | (4U << 4)); /* AF4 for both           */

	/* ---------- 3. Configure I2C1 ---------- */

	I2C1_CR2 |= (16U << 0); /* FREQ = 16 MHz (APB1 clock in MHz)        */
	I2C1_CCR |= (80U << 0); /* CCR = 80 -> 100 kHz standard mode        */
	I2C1_TRISE = 17U; /* max rise time for standard mode          */
	I2C1_CR1 |= (1U << 0); /* PE: enable the I2C1 peripheral           */

	/* ---------- UART init (USART2 on PA2) ---------- */
	RCC_AHB1ENR |= (1U << 0); /* GPIOA clock on (PA2 lives here) */
	RCC_APB1ENR |= (1U << 17); /* USART2 clock on */

	GPIOA_MODER &= ~(3U << 4); /* clear PA2 mode bits */
	GPIOA_MODER |= (2U << 4); /* PA2 = alternate function */
	GPIOA_AFRL &= ~(0xFU << 8); /* clear PA2 AF nibble */
	GPIOA_AFRL |= (7U << 8); /* AF7 = USART2 */

	USART2_BRR = 139U; /* 16MHz / 115200 ≈ 139 */
	USART2_CR1 |= (1U << 3); /* TE: transmitter enable */
	USART2_CR1 |= (1U << 13); /* UE: USART enable */

	/* Enable GPIOA clock for the onboard LED (PA5) */
	RCC_AHB1ENR |= (1U << 0); /* GPIOA clock on */
	GPIOA_MODER &= ~(3U << 10); /* clear PA5 mode bits */
	GPIOA_MODER |= (1U << 10); /* PA5 = output */

	mpu6050_write_reg(0x6B, 0x00); /* PWR_MGMT_1 = 0 -> wake from sleep */

	/* Confirm chip is alive, light LED */
	uint8_t who = mpu6050_read_reg(MPU6050_WHOAMI);
	if (who == 0x68U) {
		GPIOA_ODR |= (1U << 5); /* LED ON = success */
	}

	/* ---------- TIM2: 20 ms tick ---------- */
		RCC_APB1ENR |= (1U << 0);   /* TIM2 clock on */
		TIM2_PSC = 15999;           /* 16 MHz / 16000 = 1 kHz */
		TIM2_ARR = 19;              /* 20 ticks = 20 ms */
		TIM2_CR1 |= (1U << 0);      /* start counting */

		int16_t ax, ay, az;
		int16_t gx, gy, gz;
		char buf[128];
		int late = 0;

		float angle = 0.0f;
		float dt = 0.02f;

		uart_send_string("Calibrating, hold still...\r\n");
		float gx_bias = 0.0f;
		for (int i = 0; i < 500; i++) {
			mpu6050_read_gyro(&gx, &gy, &gz);
			gx_bias += (float)gx;
		}
		gx_bias /= 500.0f;

		snprintf(buf, sizeof(buf), "gx_bias=%d\r\n", (int)gx_bias);
		uart_send_string(buf);

		TIM2_SR &= ~(1U << 0);   /* clear any stale tick */

		uint32_t n = 0;

		for (;;) {
					if (TIM2_SR & (1U << 0)) {
						late = 1;
					} else {
						late = 0;
						while (!(TIM2_SR & (1U << 0)));
					}
					TIM2_SR &= ~(1U << 0);

					mpu6050_read_accel(&ax, &ay, &az);
					mpu6050_read_gyro(&gx, &gy, &gz);

					float accel_angle = atan2f((float)ay, (float)az) * 57.2958f;
					float gyro_rate = ((float)gx - gx_bias) / GYRO_SCALE;
					angle = ALPHA * (angle + gyro_rate * dt) + (1.0f - ALPHA) * accel_angle;

					n++;
					if (n % 50 == 0) {
						snprintf(buf, sizeof(buf), "%s tick=%lu angle=%d.%02d\r\n",
								late ? "LATE" : "ok  ", (unsigned long)(n / 50),
								(int)angle, (int)(fabsf(angle - (int)angle) * 100));
						uart_send_string(buf);
					}
				}
}
