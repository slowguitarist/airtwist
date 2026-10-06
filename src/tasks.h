#ifndef TASKS_H
#define TASKS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#define MS_SIM_BUF_MAX_SIZE	16384

/* Threads */

void fault_injector_task(void *p1, void *p2, void *p3);
void simulant_task(void *p1, void *p2, void *p3);
void sensor_task(void *p1, void *p2, void *p3);

extern struct k_sem sensor_drdy_sem;
extern struct k_msgq random_dev;
extern struct k_mutex fault_state_mutex;

extern const k_tid_t sensor_tid;
extern const k_tid_t fault_injector_tid;
extern const k_tid_t simulant_tid;

/* UART "Noise receiver" */

extern const struct device *noise_rcv;

void uart_noise_rx(const struct device *dev, void *usr);

#endif // TASKS_H