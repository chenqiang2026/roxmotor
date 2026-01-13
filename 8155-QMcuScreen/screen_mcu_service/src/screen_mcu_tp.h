#ifndef SCREEN_MCU_TP_H
#define SCREEN_MCU_TP_H
#include <semaphore.h>
#define MAX_POINT             10

typedef struct{
    sem_t notify_sem;
    sem_t mutex;
    unsigned pointer_num;	
	struct {
		uint8_t id;
		uint8_t status;
		uint16_t x;
		uint16_t y;
	} events[MAX_POINT];
} screen_mcu_tp_t;
#endif
