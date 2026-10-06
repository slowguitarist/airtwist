/*⠀⠀ ⠀⠀ ⢀⠟⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣀⠠⠤⢀⠀⠎⠀⠘⠀⢀⠠⠀⡀⠀⠀⠀⡄
⢰⣾⡖⠠⡀⠀⢻⠀⠀⠀⡏⠀⢀⠤⢾⣿⡆⠀⡇ Fault injector task is a local joker.
⠈⠉⠁⠀⠘⣀⣼⠤⠤⠤⣧⣀⠂⠀⠈⠉⠁⠀⡇ It manipulates timings and RTOS behavior
⠀⠀⠀⠀⢰⡇⢼⠆⠀⢼⡤⢹⠀⠀⠀⠀⠀⠀⡇ to stress-test time-sensitive algorithms.
⠀⠀⠀⠀⠈⣧⠈⢤⣤⠌⠀⡾         
⠀⠀⠀⠀⠀⡼⣑⠤⠤⠤⠚⢅⠀     
⠀⠀⠀⠴⠮⢒⠇⣠⠷⡀⢲⠂⠵⠖        
⠀⠀⠀⠀⠠⠮⠚⠁⠀⠑⠢⠵⠄    */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "memsonic.h"
#include "tasks.h"

void uart_noise_rx(const struct device *dev, void *usr)
{
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
}

void fault_injector_task(void *p1, void *p2, void *p3)
{
	uint8_t noise_byte;

	while (1) {
		if (k_msgq_get(&random_dev, &noise_byte, K_FOREVER) == 0) {
			k_mutex_lock(&fault_state_mutex, K_FOREVER);
			/* Process entropy byte */
			k_mutex_unlock(&fault_state_mutex);
		}
	}
}