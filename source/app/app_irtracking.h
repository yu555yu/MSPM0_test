#ifndef _APP_IRTRACKING_H_
#define _APP_IRTRACKING_H_


#include "Hardware/Board/board.h"
#include <stdio.h>
#include <stdint.h>
#include "Hardware/LCD/lcd.h"

void LineTracking_ShowRawOnLCD(void);
void LineWalking(void);
float LineTracking_GetError(void);
float LineTracking_GetRawTarget(void);

#endif

