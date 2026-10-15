/*
 * Simulant task. It is dedicated entirely to running filters.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "memsonic.h"
#include "tasks.h"
#include "zephyr/sys/printk.h"

void ekf_predict(MsXYZ acc, MsXYZ gyr, float elapsed_ms)
{
	printk("Predict, elapsed %f", (double)elapsed_ms);
	printk("A [%f, %f, %f]", (double)acc.x, (double)acc.y, (double)acc.z);
	printk("G [%f, %f, %f]", (double)gyr.x, (double)gyr.y, (double)gyr.z);
}

void ekf_update(float pressure)
{
	printk("Update, B %f", (double)pressure);
}

void simulant_task(void *, void *, void *)
{
	SimBatch sb = {0};
	uint32_t last_ts = 0;

	for (;;) {
		/* Will likely suspend in this call */
		SetKind sk = get_sim_packets(&sb);

		if (sk > 1) {
			uint32_t elapsed = sb.p[Accel].ts - last_ts;
			last_ts = sb.p[Accel].ts;
			ekf_predict(sb.p[Accel].v.xyz, sb.p[Gyro].v.xyz, elapsed);
		}

		if (sk & 1) {
			ekf_update(sb.p[Baro].v.prs);
		}
	}
}