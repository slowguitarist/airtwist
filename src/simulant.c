#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "memsonic.h"
#include "tasks.h"

struct fault_config {
	uint32_t simulated_io_delay_us;
	bool trigger_priority_inversion;
};

static struct fault_config current_faults;

void simulant_task(void *p1, void *p2, void *p3)
{
	while (1) {
		k_sem_take(&sensor_drdy_sem, K_FOREVER);

		k_mutex_lock(&fault_state_mutex, K_FOREVER);
		uint32_t io_delay = current_faults.simulated_io_delay_us;
		k_mutex_unlock(&fault_state_mutex);

		if (io_delay > 0) {
			k_busy_wait(io_delay);
		}
	}
}