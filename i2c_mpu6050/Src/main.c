#include <stdint.h>
#include <stdio.h>
#include <math.h>

/* ---------- Base addresses (from RM0390 memory map) ---------- */
#define PERIPH_BASE     (0x40000000U)

#define CPACR   (*(volatile uint32_t *)(0xE000ED88U))

#define RCC_BASE        (0x40023800U)
#define GPIOB_BASE      (0x40020400U)
#define I2C1_BASE       (0x40005400U)
#define MPU6050_ADDR        0x68U
#define MPU6050_WHOAMI      0x75U
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

#define ACCEL_SCALE   16384.0f
#define GYRO_SCALE    131.0f
#define ALPHA         0.98f

/* ---------- I2C helper functions ---------- */

static void i2c_wait_flag_sr1(uint32_t flag) {
	while (!(I2C1_SR1 & flag))
		;
}

static void i2c_start(void) {
	I2C1_CR1 |= (1U << 8);
	i2c_wait_flag_sr1(1U << 0);
}

static void i2c_stop(void) {
	I2C1_CR1 |= (1U << 9);
	while (I2C1_CR1 & (1U << 9));
}

static void i2c_send_address(uint8_t addr, uint8_t read) {
	I2C1_DR = (addr << 1) | (read & 1U);
	i2c_wait_flag_sr1(1U << 1);
	(void) I2C1_SR2;
}

static void i2c_write_byte(uint8_t data) {
	i2c_wait_flag_sr1(1U << 7);
	I2C1_DR = data;
	i2c_wait_flag_sr1(1U << 7);
}

static uint8_t i2c_read_byte(uint8_t ack) {
	if (ack)
		I2C1_CR1 |= (1U << 10);
	else
		I2C1_CR1 &= ~(1U << 10);

	i2c_wait_flag_sr1(1U << 6);
	return (uint8_t) I2C1_DR;
}

static void mpu6050_write_reg(uint8_t reg, uint8_t value) {
	i2c_start();
	i2c_send_address(MPU6050_ADDR, 0);
	i2c_write_byte(reg);
	i2c_write_byte(value);
	i2c_stop();
}

static uint8_t mpu6050_read_reg(uint8_t reg) {
	i2c_start();
	i2c_send_address(MPU6050_ADDR, 0);
	i2c_write_byte(reg);

	i2c_start();
	i2c_send_address(MPU6050_ADDR, 1);
	uint8_t value = i2c_read_byte(0);
	i2c_stop();

	return value;
}

static void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
	uint8_t xh = mpu6050_read_reg(0x3B);
	uint8_t xl = mpu6050_read_reg(0x3C);
	uint8_t yh = mpu6050_read_reg(0x3D);
	uint8_t yl = mpu6050_read_reg(0x3E);
	uint8_t zh = mpu6050_read_reg(0x3F);
	uint8_t zl = mpu6050_read_reg(0x40);

	*ax = (int16_t) ((xh << 8) | xl);
	*ay = (int16_t) ((yh << 8) | yl);
	*az = (int16_t) ((zh << 8) | zl);
}

static void mpu6050_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz) {
	uint8_t xh = mpu6050_read_reg(0x43);
	uint8_t xl = mpu6050_read_reg(0x44);
	uint8_t yh = mpu6050_read_reg(0x45);
	uint8_t yl = mpu6050_read_reg(0x46);
	uint8_t zh = mpu6050_read_reg(0x47);
	uint8_t zl = mpu6050_read_reg(0x48);

	*gx = (int16_t) ((xh << 8) | xl);
	*gy = (int16_t) ((yh << 8) | yl);
	*gz = (int16_t) ((zh << 8) | zl);
}

static void uart_send_char(char c) {
	while (!(USART2_SR & (1U << 7)))
		;
	USART2_DR = c;
}

static void uart_send_string(const char *s) {
	while (*s)
		uart_send_char(*s++);
}

/* ---------- FFT ---------- */
#define N_FFT 128

static float re[N_FFT];
static float im[N_FFT];

static void fft(void) {
	for (uint32_t i = 1, j = 0; i < N_FFT; i++) {
		uint32_t bit = N_FFT >> 1;
		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j) {
			float t;
			t = re[i]; re[i] = re[j]; re[j] = t;
			t = im[i]; im[i] = im[j]; im[j] = t;
		}
	}
	for (uint32_t len = 2; len <= N_FFT; len <<= 1) {
		float ang = -2.0f * 3.14159265f / (float)len;
		for (uint32_t i = 0; i < N_FFT; i += len) {
			for (uint32_t k = 0; k < len / 2; k++) {
				float wr = cosf(ang * (float)k);
				float wi = sinf(ang * (float)k);
				float ur = re[i + k];
				float ui = im[i + k];
				float vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
				float vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
				re[i + k] = ur + vr;
				im[i + k] = ui + vi;
				re[i + k + len / 2] = ur - vr;
				im[i + k + len / 2] = ui - vi;
			}
		}
	}
}

int main(void) {

	CPACR |= (0xFU << 20);   /* enable FPU */

	/* ---------- Enable clocks ---------- */
	RCC_AHB1ENR |= (1U << 1);
	RCC_APB1ENR |= (1U << 21);

	/* ---------- PB8 (SCL) and PB9 (SDA) ---------- */
	GPIOB_MODER &= ~((3U << 16) | (3U << 18));
	GPIOB_MODER |= ((2U << 16) | (2U << 18));
	GPIOB_OTYPER |= (1U << 8) | (1U << 9);
	GPIOB_PUPDR &= ~((3U << 16) | (3U << 18));
	GPIOB_PUPDR |= ((1U << 16) | (1U << 18));
	GPIOB_AFRH &= ~((0xFU << 0) | (0xFU << 4));
	GPIOB_AFRH |= ((4U << 0) | (4U << 4));

	/* ---------- I2C1 ---------- */
	I2C1_CR2 |= (16U << 0);
	I2C1_CCR |= (80U << 0);
	I2C1_TRISE = 17U;
	I2C1_CR1 |= (1U << 0);

	/* ---------- UART (USART2 on PA2) ---------- */
	RCC_AHB1ENR |= (1U << 0);
	RCC_APB1ENR |= (1U << 17);

	GPIOA_MODER &= ~(3U << 4);
	GPIOA_MODER |= (2U << 4);
	GPIOA_AFRL &= ~(0xFU << 8);
	GPIOA_AFRL |= (7U << 8);

	USART2_BRR = 139U;
	USART2_CR1 |= (1U << 3);
	USART2_CR1 |= (1U << 13);

	/* ---------- LED on PA5 ---------- */
	GPIOA_MODER &= ~(3U << 10);
	GPIOA_MODER |= (1U << 10);

	mpu6050_write_reg(0x6B, 0x00);

	uint8_t who = mpu6050_read_reg(MPU6050_WHOAMI);
	if (who == 0x68U) {
		GPIOA_ODR |= (1U << 5);
	}

	/* ---------- TIM2: 20 ms tick ---------- */
		RCC_APB1ENR |= (1U << 0);
		TIM2_PSC = 15999;
		TIM2_ARR = 19;
		TIM2_CR1 |= (1U << 0);

		int16_t ax, ay, az;
		int16_t gx, gy, gz;
		char buf[128];

		float angle = 0.0f;
		float dt = 0.02f;

		uart_send_string("Calibrating, hold still...\r\n");
		float gx_bias = 0.0f;
		for (int i = 0; i < 500; i++) {
			mpu6050_read_gyro(&gx, &gy, &gz);
			gx_bias += (float)gx;
		}
		gx_bias /= 500.0f;

		TIM2_SR &= ~(1U << 0);

		for (;;) {
			if (!(TIM2_SR & (1U << 0))) {
				while (!(TIM2_SR & (1U << 0)));
			}
			TIM2_SR &= ~(1U << 0);

			mpu6050_read_accel(&ax, &ay, &az);
			mpu6050_read_gyro(&gx, &gy, &gz);

			float accel_angle = atan2f((float)ay, (float)az) * 57.2958f;
			float gyro_rate = ((float)gx - gx_bias) / GYRO_SCALE;
			angle = ALPHA * (angle + gyro_rate * dt) + (1.0f - ALPHA) * accel_angle;

			/* angle sent as hundredths of a degree, so the sign survives */
			snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d\r\n",
					(int)(angle * 100.0f), ax, ay, az, gx, gy, gz);
			uart_send_string(buf);
		}
	}
