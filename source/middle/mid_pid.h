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
void PID_Init(PID_t *p);
#endif
