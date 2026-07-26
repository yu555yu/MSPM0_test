#ifndef __USER_H
#define __USER_H

void UserLED_ON(void);
void UserLED_OFF(void);
void UserLED_Toggle(void);

typedef enum {
    KEY_PRESSED = 1,
    KEY_RELEASED = 0
} KeyState;

KeyState UserKey_Read(void);

#endif