/**
 * Random noise generator using Arduino Nano 33 IoT.
 *
 * Obtains accel and gyro measurements from the LSM6DS3 IMU and
 * transforms them into a random-looking integer, which is sent
 * to the fault injector (joker) on the master.
 *
 * This code is counted as a "peripheral" because it does not
 * involve any extensive use of a RTOS.
 */

#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>
#include <Arduino_LSM6DS3.h>
#include <Wire.h>

static constexpr uint32_t SEND_INTERVAL_US = 10000ul;

void toggle_led()
{
	static bool led_on = false;
	led_on = !led_on;
	digitalWrite(LED_BUILTIN, led_on);
}

void ne_abonent()
{
	for (uint32_t k = 0;; k += 100) {
		toggle_led();
		delay(k % 500);
	}
}

bool uart_send(const uint8_t* data, size_t len)
{
    	return Serial1.write(data, len) == len;
}

bool write_imu_reg(uint8_t addr, uint8_t val)
{
	Wire.beginTransmission(0x6A);
	Wire.write(addr);
	Wire.write(val);

	return Wire.endTransmission() == 0;
}

void mix_float(uint32_t& st, float val)
{
	uint32_t bits;
	memcpy(&bits, &val, sizeof(bits));

	st = (st << 7) | (st >> 25);
	st ^= bits;
}

void setup()
{
	Serial1.begin(115200);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	if (!IMU.begin())
		ne_abonent();

	/* 1.66 kHz ODR; accel +-16 g, gyro 2000 dps. */
	if (!write_imu_reg(LSM6DS3_CTRL1_XL, 0x84))
		ne_abonent();

	if (!write_imu_reg(LSM6DS3_CTRL2_G, 0x8C))
		ne_abonent();
}

void loop()
{
	static uint64_t rng = 0xdeadbabedeadbeefull;
	static uint32_t last_us = 0;

	static uint32_t accel_noise = 0;
	static uint32_t gyro_noise = 0;

	static bool have_accel = false;
	static bool have_gyro = false;

	float x, y, z;

	if (IMU.accelerationAvailable()) {
		IMU.readAcceleration(x, y, z);
		mix_float(accel_noise, x);
		mix_float(accel_noise, y);
		mix_float(accel_noise, z);
		have_accel = true;
	}

	if (IMU.gyroscopeAvailable()) {
		IMU.readGyroscope(x, y, z);
		mix_float(gyro_noise, x);
		mix_float(gyro_noise, y);
		mix_float(gyro_noise, z);
		have_gyro = true;
	}

	if (have_accel && have_gyro) {
		rng ^= (static_cast<uint64_t>(accel_noise) << 32) | gyro_noise;

		/* xorshift64 */
		rng ^= rng >> 12;
		rng ^= rng << 25;
		rng ^= rng >> 27;

		accel_noise = 0;
		gyro_noise = 0;
		have_accel = false;
		have_gyro = false;

		const uint32_t now = micros();

		if (static_cast<uint32_t>(now - last_us) >= SEND_INTERVAL_US) {
			last_us = now;

			const uint64_t out = rng * 0x2545f4914f6cdd1dULL;

			if (uart_send(reinterpret_cast<const uint8_t*>(&out), sizeof(out)))
				toggle_led();
		}
	}
}
