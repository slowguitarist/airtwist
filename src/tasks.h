#ifndef TASKS_H
#define TASKS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include "memsonic.h"

/* Masks and sizes */

#define EXTERNAL_NOISE          1
#define JOKER_PATIENCE_MS       1000

#define MS_SIM_BUF_MAX_SIZE	32768
#define IMU_DEVICES             3

#define MS_SIM_SW_IRQ           DT_IRQN(DT_NODELABEL(lptim2))
#define MS_SIM_SW_IRQ_PRIORITY  2

#define PKT_SET_PREDICT	(BIT(Accel) | BIT(Gyro))
#define PKT_SET_UPDATE	(BIT(Baro))

/* Sensor data communication wrappers */

typedef enum {
        SetUpdate = 1,
        SetPredict = 2,
} SetKind;

typedef enum {
	Accel,
	Gyro,
	Mag,
	Baro,

        NumDevices
} PostKind;

typedef struct {
	uint32_t ts;
	PostKind kind;
	union {
		MsXYZ xyz;
		float prs;
	} v;
} SimPacket;

typedef struct {
	SimPacket p[NumDevices];
} SimBatch;

/* Threads */

void fault_injector_task(void *p1, void *p2, void *p3);
void simulant_task(void *p1, void *p2, void *p3);
void sensor_task(void *p1, void *p2, void *p3);

extern const k_tid_t sensor_task_id;
extern const k_tid_t fault_injector_task_id;
extern const k_tid_t simulant_task_id;

/*
 * Waits for any of the sets and populates the batch with at least one ready set.
 * Returns a mask indicating which set is ready.
 */
SetKind get_sim_packets(SimBatch *sb);

/* UART "Noise receiver" */

extern const struct device *noise_rcv;

void uart_noise_rx(const struct device *dev, void *usr);

#endif // TASKS_H