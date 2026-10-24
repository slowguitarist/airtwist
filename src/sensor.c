/*
 * Sensor task -- spins the measurement generator and schedules
 * simulated data-ready interrupts.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

#include "memsonic.h"
#include "profile.h"
#include "tasks.h"


/* Task-local globals */

static MsODR g_odr = {
	.acc = 5.0f,
	.gyr = 5.0f,
	.mag = 0.0f,
	.bar = 30.0f
};

static uint8_t MS_ALIGN g_sim_buf[MS_SIM_BUF_MAX_SIZE];

K_SEM_DEFINE(g_set_waker, 0, 1);

static atomic_uint_fast32_t g_stage_mask;
static atomic_uint_fast32_t g_set_drdy;

static struct k_spinlock g_inbox_lock;
static SimPacket g_inbox[NumDevices];


/* Consumer API */

/*
 * Waits for any of the sets and populates the batch with at least one ready set.
 * Returns a mask indicating which set is ready.
 */
SetKind get_sim_packets(SimBatch *sb)
{
	k_sem_take(&g_set_waker, K_FOREVER);
	/* Semaphore guarantees happens-before */
	SetKind sk = atomic_load_explicit(&g_set_drdy, memory_order_relaxed);

	k_spinlock_key_t key = k_spin_lock(&g_inbox_lock);
	
	if (sk & 1) {
		sb->p[Baro] = g_inbox[Baro];
	}

	if (sk > 1) {
		sb->p[Accel] = g_inbox[Accel];
		sb->p[Gyro] = g_inbox[Gyro];
	}

	k_spin_unlock(&g_inbox_lock, key);
	return sk;
}


/* Producer helpers */

static inline void append_targets(MsSim *sim)
{
	size_t targets = sizeof(flight_profile) / sizeof(flight_profile[0]);

  	for (size_t i = 0; i < targets; ++i) {
		ms_sim_fix(
			sim,
			flight_profile[i].dur,
			&flight_profile[i].a,
			&flight_profile[i].g
		);
  	}
}

static inline uint32_t calculate_polling_rate(MsODR *odr)
{
	if (odr->acc <= 0.f)
		odr->acc = 0.f;
	if (odr->gyr <= 0.f)
		odr->gyr = 0.f;
	if (odr->mag <= 0.f)
		odr->mag = 0.f;
	if (odr->bar <= 0.f)
		odr->bar = 0.f;

	/* Divide by 5 to avoid falling behind on calculations.
	 * 0 would yield instead of sleeping, which is necessary to keep simulation running.
	 */
	return (uint32_t)(MIN(odr->acc, MIN(odr->gyr, MIN(odr->mag, odr->bar))) / 5);
}


/* Producer */

/*
 * This ISR introduces a redundant synchronization point, but is necessary
 * for a complete simulation.
 */
static void sim_readiness_isr(const void *)
{
	uint32_t drdy = 0;
	const uint32_t mask =
		atomic_exchange_explicit(&g_stage_mask, 0, memory_order_acq_rel);

	if (mask & PKT_SET_PREDICT)
		drdy += SetPredict;

	if (mask & PKT_SET_UPDATE)
		drdy += SetUpdate;

	if (drdy != 0) {
		/* Semaphore guarantees happens-before */
		atomic_store_explicit(&g_set_drdy, drdy, memory_order_relaxed);
		k_sem_give(&g_set_waker);
	}
}

static inline void async_post_reading(uint32_t ts, SimPacket pkt)
{
	pkt.ts = ts;

	k_spinlock_key_t key = k_spin_lock(&g_inbox_lock);
	g_inbox[pkt.kind] = pkt;
	k_spin_unlock(&g_inbox_lock, key);

	atomic_fetch_or_explicit(&g_stage_mask, BIT(pkt.kind), memory_order_release);
	// Fake latch source.
	k_irq_set_pending(MS_SIM_SW_IRQ);
}

void sensor_task(void *, void *, void *)
{
	printk("Starting sensor task.\n");

	SimPacket buf[NumDevices] = {
		[0] = {.kind = Accel},
		[1] = {.kind = Gyro},
		[2] = {.kind = Mag},
		[3] = {.kind = Baro}
	};

	MsReady local_drdy[NumDevices] = {0};

	const uint32_t poll_interval_ms = calculate_polling_rate(&g_odr);

	IRQ_CONNECT(MS_SIM_SW_IRQ, MS_SIM_SW_IRQ_PRIO, sim_readiness_isr, NULL, 0);
	k_irq_enable(MS_SIM_SW_IRQ);

	MsSim *sim = ms_sim_new_skewed(g_sim_buf, &g_odr, 0.5f, MS_ENV_FURNAS, 1000);

  	append_targets(sim);

  	for (;;) {
		const uint32_t ts = k_uptime_get_32();

		/* The first call will propagate simulation to "now".
		 */
    		local_drdy[Accel] = ms_sim_accel(sim, ts, &buf[Accel].v.xyz);
		local_drdy[Gyro] = ms_sim_angvel(sim, ts, &buf[Gyro].v.xyz);
		local_drdy[Mag] = ms_sim_magfield(sim, ts, &buf[Mag].v.xyz);
		local_drdy[Baro] = ms_sim_pressure(sim, ts, &buf[Baro].v.prs);

		for (PostKind i = 0; i < NumDevices; ++i) {
			if (local_drdy[i].val == MS_READY_NEW.val)
				async_post_reading(ts, buf[i]);
		}

    		k_msleep(poll_interval_ms);
  	}
}