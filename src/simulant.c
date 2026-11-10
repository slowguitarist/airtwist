/*
 * Simulant task. It is dedicated entirely to running filters.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <math.h>

#include "memsonic.h"
#include "tasks.h"


#define EKF_STATE 6
#define EKF_OMEGA 4

#define EKF_BARO_VARIANCE 0.1f
#define GRAVITY_SI        9.81f

#define EKF_BIAS_GYRO_X   0.0f
#define EKF_BIAS_GYRO_Y   0.0f
#define EKF_BIAS_GYRO_Z   0.0f
#define EKF_BIAS_ACCL_Z   0.0f


typedef struct {
        float x[EKF_STATE];
        float P[EKF_STATE * EKF_STATE];
        float q[4];
} eskf_t;

static const float Q_DIAG[EKF_STATE] = {
        1e-2f,  // pz
        1e-2f,  // vz
        1e-7f,  // baz
        1e-10f, // bgx
        1e-10f, // bgy
        1e-10f  // bgz
};


static void mat4_vec_mul(const float A[16], const float x[4], float y[4]) {
        for (int i = 0; i < 4; i++) {
                y[i] = 0.0f;
                for (int j = 0; j < 4; j++) {
                        y[i] += A[i * 4 + j] * x[j];
                }
        }
}

static void quat_normalize(float q[4]) {
        float norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
        if (norm > 0.0f) {
                float inv = 1.0f / norm;
                for (int i = 0; i < 4; i++) {
                        q[i] *= inv;
                }
        }
}

static void eskf_init(eskf_t *f) {
        f->x[0] = 0.0f; // pz
        f->x[1] = 0.0f; // vz
        f->x[2] = EKF_BIAS_ACCL_Z;
        f->x[3] = EKF_BIAS_GYRO_X;
        f->x[4] = EKF_BIAS_GYRO_Y;
        f->x[5] = EKF_BIAS_GYRO_Z;
	
        f->q[0] = 1.0f;
        f->q[1] = 0.0f;
        f->q[2] = 0.0f;
        f->q[3] = 0.0f;

        for (int i = 0; i < EKF_STATE * EKF_STATE; i++) {
                f->P[i] = 0.0f;
        }
        for (int i = 0; i < EKF_STATE; i++) {
                f->P[i * EKF_STATE + i] = 1.0f;
        }
}

static void eskf_predict(eskf_t *f, const float u[6], float dt) {
        float wx = u[3] - f->x[3];
        float wy = u[4] - f->x[4];
        float wz = u[5] - f->x[5];

        const float Omega[16] = {
                0.0f, -wx, -wy, -wz,
                wx,  0.0f, wz, -wy,
                wy, -wz,  0.0f, wx,
                wz,  wy, -wx,  0.0f
        };

        float q_dot[4];
        mat4_vec_mul(Omega, f->q, q_dot);

        for (int i = 0; i < 4; i++) {
                f->q[i] += 0.5f * dt * q_dot[i];
        }
        quat_normalize(f->q);

        float q0 = f->q[0], q1 = f->q[1], q2 = f->q[2], q3 = f->q[3];

        float r13 = 2.0f * (q1 * q3 - q0 * q2);
        float r23 = 2.0f * (q2 * q3 + q0 * q1);
        float r33 = 1.0f - 2.0f * (q1 * q1 + q2 * q2);

        float a_vert = (r13 * u[0] + r23 * u[1] + r33 * u[2]);

        f->x[0] += dt * f->x[1];
        f->x[1] += dt * (a_vert - f->x[2] - GRAVITY_SI);

        float P_next[EKF_STATE * EKF_STATE];
        for (int i = 0; i < EKF_STATE * EKF_STATE; i++) {
                P_next[i] = f->P[i];
        }

        for (int j = 0; j < EKF_STATE; j++) {
                P_next[j] += dt * f->P[EKF_STATE + j];
        }
        for (int i = 0; i < EKF_STATE; i++) {
                P_next[i * EKF_STATE + 1] += dt * P_next[i * EKF_STATE];
        }

        for (int i = 0; i < EKF_STATE * EKF_STATE; i += (EKF_STATE + 1)) {
                P_next[i] += Q_DIAG[i / (EKF_STATE + 1)];
        }
        for (int i = 0; i < EKF_STATE * EKF_STATE; i++) {
                f->P[i] = P_next[i];
        }
}

static void eskf_update(eskf_t *f, float z) {
        float y = z - f->x[0];
        float S_inv = 1.0f / (f->P[0] + EKF_BARO_VARIANCE);

        float K[EKF_STATE];
        for (int i = 0; i < EKF_STATE; i++) {
                K[i] = f->P[i * EKF_STATE] * S_inv;
                f->x[i] += K[i] * y;
        }

        for (int i = 0; i < EKF_STATE; i++) {
                for (int j = 0; j < EKF_STATE; j++) {
                        f->P[i * EKF_STATE + j] -= K[i] * f->P[j];
                }
        }
}

void simulant_task(void *, void *, void *) {
        printk("Starting simulant task.\n");

	eskf_t ekf;
        eskf_init(&ekf);

        SimBatch sb = {0};
        uint32_t last_ts = 0;
	bool pred_init = false;

        for (;;) {
                SetKind sk = get_sim_packets(&sb);

                if ((sk & SetPredict) == SetPredict) {
                        uint32_t current_ts = sb.p[Accel].ts;

                        if (!pred_init) {
                                last_ts = current_ts;
                                pred_init = true;
                                continue;
                        }

                        uint32_t elapsed = current_ts - last_ts;
                        last_ts = current_ts;

                        float dt = (elapsed > 0) ? ((float)elapsed / 1000.0f) : 0.01f;

                        float u[6] = {
                                sb.p[Accel].v.xyz.x,
                                sb.p[Accel].v.xyz.y,
                                sb.p[Accel].v.xyz.z,
                                sb.p[Gyro].v.xyz.x,
                                sb.p[Gyro].v.xyz.y,
                                sb.p[Gyro].v.xyz.z
                        };

                        eskf_predict(&ekf, u, dt);
                }

                if ((sk & SetUpdate) == SetUpdate) {
                        float z = sb.p[Baro].v.prs;
                        eskf_update(&ekf, z);
                }
        }

	printk(
		"[ESKF] pz: %f, vz: %f, quat: %f %f %f %f\n",
		(double)ekf.x[0],
		(double)ekf.x[1],
		(double)ekf.q[0],
		(double)ekf.q[1],
		(double)ekf.q[2],
		(double)ekf.q[3]
	);
}