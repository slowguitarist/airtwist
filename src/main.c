/*
 * Simulation entry -- threads, global definitions, and ISR setup.
 */

#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "zephyr/sleep.h"
#include "zephyr/sys/printk.h"

#include "tasks.h"

void memsonic_panic_report(const uint8_t *file, size_t len, uint32_t line)
{
	printk("Rust panic at %.*s:%u\n", (int)len, (const char *)file, line);
}


K_THREAD_DEFINE(
	sensor_task_id,
	SENSOR_TASK_STACK,
	sensor_task,
	NULL,
	NULL,
	NULL,
	FAIR_TASK_PRIO,
	0,
	0
);
K_THREAD_DEFINE(
	fault_injector_task_id,
	FAIR_TASK_STACK,
	fault_injector_task,
	NULL,
	NULL,
	NULL,
	FAIR_TASK_PRIO,
	0,
	0
);
K_THREAD_DEFINE(
	simulant_task_id,
	FAIR_TASK_STACK,
	simulant_task,
	NULL,
	NULL,
	NULL,
	FAIR_TASK_PRIO,
	0,
	0
);
K_THREAD_DEFINE(
	pi_impsoter_task_id,
	FAIR_TASK_STACK,
	pi_imposter_task,
        NULL,
	NULL,
	NULL,
	PI_IMPOSTER_PRIO,
	0,
	0
);

#if EXTERNAL_NOISE > 0
/*
 * Simulated random device via message queue from ISR to fault injector.
 */
K_MSGQ_DEFINE(random_dev, sizeof(uint64_t), 64, sizeof(uint64_t));

/*
 * UART receiver structure (initialized by Zephyr).
 */
const struct device *noise_rcv;

#endif /* EXTERNAL_NOISE */

int main(void)
{
	printk("Starting the simulation...\n");

#if EXTERNAL_NOISE > 0
	noise_rcv = DEVICE_DT_GET(DT_ALIAS(noisegen));
	if (!device_is_ready(noise_rcv))
		return -ENODEV;

	uart_irq_callback_set(noise_rcv, uart_noise_rx);
	uart_irq_rx_enable(noise_rcv);
#endif /* EXTERNAL_NOISE */

	return 0;
}