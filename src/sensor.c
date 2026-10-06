#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "memsonic.h"
#include "tasks.h"

void sensor_task(void *p1, void *p2, void *p3)
{
	static uint8_t MS_ALIGN sim_buf[MS_SIM_BUF_MAX_SIZE];
	MsODR odr = { .acc = 5.0f, .gyr = 5.0f, .mag = 0.0f, .bar = 30.0f };
	MsSim *sim = ms_sim_new_skewed(sim_buf, &odr, 0.1f, MS_ENV_FURNAS, 0);

	uint32_t start_time = k_uptime_get_32();
	MsXYZ dummy_out;

	while (1) {
		uint32_t now = k_uptime_get_32() - start_time;
		MsReady ready = ms_sim_accel(sim, now, &dummy_out);

		if (ready.val == MS_READY_NEW.val) {
			k_sem_give(&sensor_drdy_sem);
		}

		k_msleep(2);
	}
}