/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"   // GPIO引脚宏定义的头文件

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as   （引脚可以配置成的几种模式：
        * Analog       模拟模式（ADC采样等）
        * Input        输入模式（读按键、读传感器）
        * Output       输出模式（点灯、驱动蜂鸣器等）
        * EVENT_OUT    事件输出
        * EXTI         外部中断模式）
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};   // 定义配置结构体：先把所有参数清零，再逐个填写

  /* GPIO Ports Clock Enable（给GPIO口打开时钟，有了时钟，引脚才能工作） */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level（配置引脚的初始输出电平） */
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9|GPIO_PIN_10, GPIO_PIN_SET);   // PF9/PF10初始给高电平 → 两个LED熄灭（低电平点亮）

  /*Configure GPIO pin : PE4（KEY0按键） */
  GPIO_InitStruct.Pin = GPIO_PIN_4;              // 选引脚：PE4（KEY0按键）
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // 模式：普通输入（按键扫描函数用查询方式读电平，不用中断，查询更安全）
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // 内部上拉：没按键的时候默认是高电平
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);        // 把上面的配置写进GPIOE的寄存器，立即生效

  /*Configure GPIO pin : PE3（KEY1按键，播放/停止歌曲） */
  GPIO_InitStruct.Pin = GPIO_PIN_3;              // 选引脚：PE3（KEY1按键）
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // 模式：普通输入（按键扫描函数查询读取电平，不占中断）
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // 内部上拉：没按键的时候默认是高电平
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);        // 生效

  /*Configure GPIO pin : PE2（KEY2按键，减慢流水灯） */
  GPIO_InitStruct.Pin = GPIO_PIN_2;              // 选引脚：PE2（KEY2按键）
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // 模式：普通输入（查询读取）
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // 内部上拉：没按键的时候默认是高电平
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);        // 生效

  /*Configure GPIO pins : PF9 PF10（流水灯的两个LED引脚） */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;  // 选引脚：PF9和PF10，用"|"把两个引脚合起来一次配置
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    // 模式：推挽输出（能输出高电平和低电平）
  GPIO_InitStruct.Pull = GPIO_PULLUP;            // 内部上拉：对输出模式影响不大
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;   // 翻转速度：低速即可，点灯不需要高速
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);        // 写进GPIOF的寄存器，立即生效

  /* PF8（蜂鸣器引脚）不再配置成普通GPIO输出！
     现在PF8复用为TIM13_CH1（定时器的PWM输出），用来输出不同频率的方波奏乐，
     配置代码在tim.c的HAL_TIM_MspPostInit()函数里 */

  /*Configure GPIO pin : PA0（WK_UP按键，加快流水灯） */
  GPIO_InitStruct.Pin = GPIO_PIN_0;              // 选引脚：PA0（WK_UP按键）
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;        // 模式：普通输入（查询读取）
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;          // 内部下拉：WK_UP按下接3.3V（高电平），松开后默认拉低
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);        // 生效

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
