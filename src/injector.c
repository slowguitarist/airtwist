/*⠀⠀ ⠀⠀ ⢀⠟⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣀⠠⠤⢀⠀⠎⠀⠘⠀⢀⠠⠀⡀⠀⠀⠀⡄
⢰⣾⡖⠠⡀⠀⢻⠀⠀⠀⡏⠀⢀⠤⢾⣿⡆⠀⡇ Fault injector task is a local joker.
⠈⠉⠁⠀⠘⣀⣼⠤⠤⠤⣧⣀⠂⠀⠈⠉⠁⠀⡇ It manipulates timings and RTOS behavior
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

K_MSGQ_DEFINE(random_dev, sizeof(uint64_t), 4, sizeof(uint64_t));

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

static inline void inject_random_fault(uint64_t seed)
{

}

void fault_injector_task(void *p1, void *p2, void *p3)
{
	uint64_t seed = 0xdeadfa11deafbeef;
	int st = 1;

	while (1) {
		#if EXTERNAL_NOISE > 0
		st = k_msgq_get(&random_dev, &seed, K_MSEC(JOKER_PATIENCE_MS));
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