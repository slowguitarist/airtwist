/*⠀⠀ ⠀⠀ ⢀⠟⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣀⠠⠤⢀⠀⠎⠀⠘⠀⢀⠠⠀⡀⠀⠀⠀⡄
⢰⣾⡖⠠⡀⠀⢻⠀⠀⠀⡏⠀⢀⠤⢾⣿⡆⠀⡇ Fault injector task is a local joker.
⠈⠉⠁⠀⠘⣀⣼⠤⠤⠤⣧⣀⠂⠀⠈⠉⠁⠀⡇ It manipulates timings and interrupts
⠀⠀⠀⠀⢰⡇⢼⠆⠀⢼⡤⢹⠀⠀⠀⠀⠀⠀⡇ to stress-test time-sensitive algorithms.
⠀⠀⠀⠀⠈⣧⠈⢤⣤⠌⠀⡾         
⠀⠀⠀⠀⠀⡼⣑⠤⠤⠤⠚⢅⠀     
⠀⠀⠀⠴⠮⢒⠇⣠⠷⡀⢲⠂⠵⠖        
⠀⠀⠀⠀⠠⠮⠚⠁⠀⠑⠢⠵⠄    */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "tasks.h"
#include "zephyr/irq.h"
#include "zephyr/sleep.h"

K_MSGQ_DEFINE(random_dev, sizeof(uint64_t), 4, sizeof(uint64_t));
/*
 * OS-level sync for controlled priority inversions.
 */
K_MUTEX_DEFINE(pi_mutex);
K_SEM_DEFINE(pi_request, 0, 1);
K_SEM_DEFINE(pi_locked, 0, 1);
/*
 * For how long should PI last, set by FI.
 */
static volatile uint32_t pi_duration_ms = 0;

/* UART receiver ISR */

/*
 * ISR that deposits random numbers obtained from Arduino IMU.
 */
void uart_noise_rx(const struct device *dev, void *usr)
{
#if EXTERNAL_NOISE > 0
	static uint8_t buf[8];
	static uint8_t idx = 0;
	
	uint8_t c = 0;

	uart_irq_update(dev);

	if (!uart_irq_rx_ready(dev))
		return;
	
	while (uart_fifo_read(dev, &c, 1) == 1) {
		buf[idx] = c;
		
		if (++idx < 8) 
			continue;

		idx = 0;
		uint64_t r = 0;
		
		for (int i = 0; i < 8; ++i) 
			r |= ((uint64_t)buf[i] << (i << 3));
		
		k_msgq_put(&random_dev, &r, K_NO_WAIT);
	}
#endif /* EXTERNAL_NOISE */
}


/* Randomness helpers */

static inline uint64_t next_rand(uint64_t *k)
{
	/* Splitmix64 - similar to xorshift:
	 * https://rosettacode.org/wiki/Pseudo-random_numbers/Splitmix64
	 */
	uint64_t z = (*k += (uint64_t)(0x9e3779b97f4a7c15));
	z = (z ^ (z >> 30)) * (uint64_t)(0xbf58476d1ce4e5b9);
	z = (z ^ (z >> 27)) * (uint64_t)(0x94d049bb133111eb);

    	return z ^ (z >> 31);
}

static inline uint32_t rand_range(uint64_t *k, uint32_t min, uint32_t max)
{
	uint64_t span = (uint64_t)max - min + 1;
	return min + (uint32_t)(next_rand(k) % span);
}


/* Fault injector */

void pi_imposter_task(void *, void *, void *)
{
	for (;;) {
		/* Wait until PI is requested by master. */
		k_sem_take(&pi_request, K_FOREVER);

		/* Inversion occurs on this lock */
		k_mutex_lock(&pi_mutex, K_FOREVER);
		k_sem_give(&pi_locked);
		k_sleep(K_MSEC(pi_duration_ms));
		k_mutex_unlock(&pi_mutex);
	}
}

static inline void inject_random_fault(uint64_t seed)
{
	if (next_rand(&seed) % 100U >= FI_INJECT_PROB) {
		k_sleep(K_MSEC(rand_range(&seed, FI_IDLE_MIN_MS, FI_IDLE_MAX_MS)));
		return;
	}

	switch (next_rand(&seed) % 3U) {
	case 0:
		uint32_t ms = rand_range(&seed, FI_BLOCK_MIN_MS, FI_BLOCK_MAX_MS);
		k_busy_wait(ms * 1000U);
		break;
	case 1:
		pi_duration_ms = rand_range(&seed, FI_PI_MIN_MS, FI_PI_MAX_MS);
		k_sem_give(&pi_request);
		k_sem_take(&pi_locked, K_FOREVER);
		k_mutex_lock(&pi_mutex, K_FOREVER);
		k_mutex_unlock(&pi_mutex);
		break;
	case 2:
		irq_disable(MS_SIM_SW_IRQ);
		k_sleep(K_MSEC(rand_range(&seed, FI_IRQ_MIN_MS, FI_IRQ_MAX_MS)));
		irq_enable(MS_SIM_SW_IRQ);
		break;
	}
}

void fault_injector_task(void *p1, void *p2, void *p3)
{
	uint64_t seed = 0xdeadfa11deafbeef;
	int st = 1;

	for (;;) {
		#if EXTERNAL_NOISE > 0
		st = k_msgq_get(&random_dev, &seed, K_MSEC(FI_PATIENCE_MS));
		#endif

		/* Fallback to xorshift if Arduino timed out */
		if (st != 0) {
			seed ^= seed << 13;
			seed ^= seed >> 17;
			seed ^= seed << 5;
		}

		inject_random_fault(seed);
	}
}