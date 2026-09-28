#ifndef __KEY_H
#define __KEY_H
#include "main.h"   // 需要用到HAL库的GPIO函数

#define KEY0 HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_4) //KEY0按键引脚（PE4），按下为低电平
#define KEY1 HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_3) //KEY1按键引脚（PE3），按下为低电平
#define KEY2 HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_2) //KEY2按键引脚（PE2），按下为低电平
#define WK_UP HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0) //WK_UP按键引脚（PA0），按下为高电平（和KEY0/1/2相反！）

#define KEY0_PRES 	1

extern uint8_t Key0_flag;   // KEY0按键事件标志
extern uint8_t Key1_flag;   // KEY1按键事件标志
extern uint8_t Key2_flag;   // KEY2按键事件标志
extern uint8_t WKUP_flag;   // WK_UP按键事件标志

uint8_t KEY_Scan(uint8_t);  	//KEY0(PE4)按键扫描函数：按下返回1，没按返回0
uint8_t KEY1_Scan(uint8_t);  	//KEY1(PE3)按键扫描函数：按下返回1，没按返回0
uint8_t KEY2_Scan(uint8_t);  	//KEY2(PE2)按键扫描函数：按下返回1，没按返回0
uint8_t WKUP_Scan(uint8_t);  	//WK_UP(PA0)按键扫描函数：按下返回1，没按返回0

#endif
