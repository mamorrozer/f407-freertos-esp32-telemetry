#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

#define USER_LED_GPIO_Port GPIOB
#define USER_LED_Pin GPIO_PIN_2

extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart1;

void Error_Handler(void);

#endif
