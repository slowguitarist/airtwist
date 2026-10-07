/*
 * Simulation entry -- threads, global definitions, and ISR setup.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "tasks.h"


K_THREAD_DEFINE(
	sensor_task_id,
	1024,
	sensor_task,
	NULL,
	NULL,
	NULL,
	5,
	0,
	0
);
K_THREAD_DEFINE(
	fault_injector_task_id,
	1024,
	fault_injector_task,
	NULL,
	NULL,
	NULL,
	5,
	0,
	0
);
K_THREAD_DEFINE(
	simulant_task_id,
	2048,
	simulant_task,
	NULL,
	NULL,
	NULL,
	5,
	0,
	0
);

/*
 * Simulated random device via message queue from ISR to fault injector.
 */
K_MSGQ_DEFINE(random_dev, sizeof(uint64_t), 64, 8);

/*
 * UART receiver structure (initialized by Zephyr).
 */
const struct device *noise_rcv;

int main(void)
{
	noise_rcv = DEVICE_DT_GET(DT_ALIAS(noisegen));
	if (!device_is_ready(noise_rcv))
		return -ENODEV;

	uart_irq_callback_set(noise_rcv, uart_noise_rx);
	uart_irq_rx_enable(noise_rcv);

	return 0;
}