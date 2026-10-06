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

#include <stdint.h>

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_LSM6DS3.h>
#include <SAMDTimerInterrupt.h>

#include "cmsis_gcc.h"

SAMDTimer ITimer(TIMER_TC3);

union plain_old_bytes {
	float f;
	uint32_t d;
};

struct noise_data {
	volatile uint32_t accl;
	volatile uint32_t gyro;
};

struct async_flags {
	volatile uint8_t data;
	volatile uint8_t event;
	volatile bool uart_ok = false;
	volatile bool uart_busy = false;
	volatile bool led_on = false;
};

static constexpr uint8_t ACCL_MASK = 1u << 0;
static constexpr uint8_t GYRO_MASK = 1u << 1;
static constexpr uint8_t READY = ACCL_MASK | GYRO_MASK;

static constexpr uint8_t EV_DATA = 1u << 0;
static constexpr uint8_t EV_UART = 1u << 1;

static volatile struct async_flags g_flags;
static volatile struct noise_data g_data;


/* IMU ISR */

/*
 * "Since when has the IMU become a random device?"
 * 	-- Nathan H. Carvey, 1968
 */
void random_device_isr()
{
	union plain_old_bytes x, y, z;

	if (IMU.accelerationAvailable()) {
		IMU.readAcceleration(x.f, y.f, z.f);

		const uint32_t k = g_data.accl;
		g_data.accl ^= ((x.d << (k & 15u)) | y.d) ^ z.d;

		g_flags.data |= ACCL_MASK;
	}

	if (IMU.gyroscopeAvailable()) {
		IMU.readGyroscope(x.f, y.f, z.f);

		const uint32_t k = g_data.gyro;
		g_data.gyro ^= ((y.d << (k & 15u)) | z.d) ^ x.d;

		g_flags.data |= GYRO_MASK;
	}

	if ((g_flags.data & READY) == READY) {
		g_flags.event |= EV_DATA;

		/* We went to enforce setting flags before SEV */
		__DMB();

		/* "Send Event" wakes the MCU from WFI */
		__SEV();
	}
}


/* Helper functions */

void toggle_led()
{
	g_flags.led_on = !g_flags.led_on;
	digitalWrite(LED_BUILTIN, g_flags.led_on);
}

void doom_blink()
{
	pinMode(LED_BUILTIN, OUTPUT);

	for (uint64_t k = 0; ; k += 100) {
		toggle_led();
		delay(k % 500);
	}
}


bool uart_send(const uint8_t* data, size_t length)
{
	const size_t w = Serial1.write(data, length);
	const bool ok = w == length;
	return ok;
}

bool write_imu_reg(uint8_t addr, uint8_t val)
{
	Wire.beginTransmission(0x6A);
	Wire.write(addr);
	Wire.write(val);

	return Wire.endTransmission() == 0;
}


/* Configuration */

void setup()
{
	/* This is UART to Nucleo */
	Serial1.begin(115200);

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	if (!IMU.begin())
		doom_blink();

	/* Because we are generating random numbers, we'd like to
   	 * crank up the noise of the LSM (1.6kHz ODR / 16g range).
   	 */
	if (!write_imu_reg(LSM6DS3_CTRL1_XL, 0x84))
		doom_blink();

	if (!write_imu_reg(LSM6DS3_CTRL2_G, 0x8C))
		doom_blink();

	if (!ITimer.attachInterruptInterval(1000, random_device_isr))
		doom_blink();
}


/* Main task -- UART TX endpoint */

void loop()
{
	static uint64_t rng = 0xdeadc0ffeebeefffULL;

	uint8_t events;
	bool ready = false;
	uint32_t r_accl = 0;
	uint32_t r_gyro = 0;

	noInterrupts();

	events = g_flags.event;
	g_flags.event = 0;

	if ((g_flags.data & READY) == READY) {
		g_flags.data &= static_cast<uint8_t>(~READY);
		g_data.accl = r_accl;
		g_data.gyro = r_gyro;
		ready = true;
	}

	interrupts();

	if ((events & EV_DATA) && ready) {
		rng ^= static_cast<uint64_t>(r_accl) << 32;
		rng ^= static_cast<uint64_t>(r_gyro);

		const uint64_t copy = rng;

		uart_send(
			reinterpret_cast<const uint8_t*>(&copy),
			sizeof(copy)
		);
	}

	/* Wait for an interrupt from the ISR */
	__WFI();
}
