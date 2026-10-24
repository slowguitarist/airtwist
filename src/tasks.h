#ifndef TASKS_H
#define TASKS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include "memsonic.h"

/* Masks and sizes */

#define FAIR_TASK_PRIO		5
#define FAIR_TASK_STACK		8192

#define SENSOR_TASK_STACK	131072

#define EXTERNAL_NOISE          1

#define MS_SIM_BUF_MAX_SIZE	32768
#define IMU_DEVICES             3

#define MS_SIM_SW_IRQ           DT_IRQN(DT_NODELABEL(lptim2))
#define MS_SIM_SW_IRQ_PRIO	2

#define PKT_SET_PREDICT	(BIT(Accel) | BIT(Gyro))
#define PKT_SET_UPDATE	(BIT(Baro))

#define FI_PATIENCE_MS       	400
#define FI_INJECT_PROB		25
#define FI_IDLE_MIN_MS      	10
#define FI_IDLE_MAX_MS      	100
#define FI_BLOCK_MIN_MS     	5
#define FI_BLOCK_MAX_MS     	50
#define FI_PI_MIN_MS        	5
#define FI_PI_MAX_MS        	50
#define FI_IRQ_MIN_MS       	5
#define FI_IRQ_MAX_MS       	20
#define PI_IMPOSTER_PRIO	(FAIR_TASK_PRIO + 1)

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
void pi_imposter_task(void *, void *, void *);

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

extern struct k_msgq random_dev;

void uart_noise_rx(const struct device *dev, void *usr);

#endif // TASKS_H