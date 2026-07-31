#ifndef	__MID_PID_H__
#define __MID_PID_H__


typedef struct {
	float Target;
	float Actual;
	float Actual1;
	float Out;
	
	float Kp;
	float Ki;
	float Kd;
	
	float Error0;
	float Error1;
	float ErrorInt;
	
	float ErrorIntMax;
	float ErrorIntMin;
	
	float OutMax;
	float OutMin;
} PID_t;

void PID_Update(PID_t *p);
/*
 * Position PID with an externally measured actual rate.
 * Used by the camera ball controller so the D term uses velocity_cm_s
 * directly instead of differentiating the discrete camera position signal.
 */
void PID_UpdateWithRate(PID_t *p, float actual_rate, float dt_s);
void PID_Init(PID_t *p);
#endif
