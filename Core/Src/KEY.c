#include "key.h"   // 按键模块头文件：提供4个按键的引脚宏定义和扫描函数声明

//按键处理函数
//返回按键值
//mode:0,不支持连续按;1,支持连续按;   ← mode=0：按住不放只算按一次；mode=1：按住不放会连续触发
//0，没有任何按键按下
//1，KEY2按下
//2，KEY3按下

uint8_t Key0_flag=0;   // KEY0按键事件标志：KEY_Scan()检测到按下后返回1，main里处理完记得清0
uint8_t Key1_flag=0;   // KEY1按键事件标志：KEY1_Scan()检测到按下后返回1，main里处理完记得清0
uint8_t Key2_flag=0;   // KEY2按键事件标志：KEY2_Scan()检测到按下后返回1，main里处理完记得清0
uint8_t WKUP_flag=0;   // WK_UP按键事件标志：WKUP_Scan()检测到按下后返回1，main里处理完记得清0

//KEY0(PE4)按键扫描函数：按下返回1，没按返回0
uint8_t KEY_Scan(uint8_t mode)
{
	static uint8_t key_up=1;//按键松开标志：1=上一次已松开，0=上一次还按着（static让变量跨调用保持）

	if(mode) key_up=1;  //支持连按(连续按有效)：mode=1时每次都当成"松开状态"，允许按住连续触发

	if(key_up && KEY0==0)      // 条件：上一次是松开状态 且 现在引脚是低电平（KEY0按下时引脚接地变低）
	{
		HAL_Delay(10);         // 消抖：等10ms再读一次，滤掉按键按下瞬间的电平抖动毛刺
		key_up = 0;            // 标记"本次按下已处理"，防止按住不动时反复触发
		if(KEY0==0) return 1;  // 10ms后还是低电平 → 确认真的按下，返回1
	}
	else if(KEY0==1) key_up=1; // 引脚回到高电平（按键松开了）→ 复位标志，下次按下才能再次触发
	return 0;// 无按键按下，返回0
}

//KEY1(PE3)按键扫描函数：按下返回1，没按返回0（原理和KEY_Scan完全一样，只是读的引脚不同）
uint8_t KEY1_Scan(uint8_t mode)
{
	static uint8_t key_up=1;//按键松开标志：1=上一次已松开，0=上一次还按着

	if(mode) key_up=1;  //支持连按(连续按有效)

	if(key_up && KEY1==0)      // 上一次是松开状态 且 现在引脚是低电平（按下）
	{
		HAL_Delay(10);         // 消抖：等10ms再读一次
		key_up = 0;            // 标记"本次按下已处理"，防止按住不动时反复触发
		if(KEY1==0) return 1;  // 10ms后还是低电平 → 确认真的按下，返回1
	}
	else if(KEY1==1) key_up=1; // 引脚回到高电平（松开了）→ 复位标志
	return 0;// 无按键按下，返回0
}

//KEY2(PE2)按键扫描函数：按下返回1，没按返回0（和KEY0/KEY1一样，按下是低电平）
uint8_t KEY2_Scan(uint8_t mode)
{
	static uint8_t key_up=1;//按键松开标志：1=上一次已松开，0=上一次还按着

	if(mode) key_up=1;  //支持连按(连续按有效)

	if(key_up && KEY2==0)      // 上一次是松开状态 且 现在引脚是低电平（按下）
	{
		HAL_Delay(10);         // 消抖：等10ms再读一次
		key_up = 0;            // 标记"本次按下已处理"，防止按住不动时反复触发
		if(KEY2==0) return 1;  // 10ms后还是低电平 → 确认真的按下，返回1
	}
	else if(KEY2==1) key_up=1; // 引脚回到高电平（松开了）→ 复位标志
	return 0;// 无按键按下，返回0
}

//WK_UP(PA0)按键扫描函数：按下返回1，没按返回0
//注意：WK_UP按键按下是高电平（3.3V），和KEY0/1/2（按下接地变低）正好相反，所以判断条件相反
uint8_t WKUP_Scan(uint8_t mode)
{
	static uint8_t key_up=1;//按键松开标志：1=上一次已松开，0=上一次还按着

	if(mode) key_up=1;  //支持连按(连续按有效)

	if(key_up && WK_UP==1)     // 上一次是松开状态 且 现在引脚是高电平（WK_UP按下接到3.3V变高）
	{
		HAL_Delay(10);         // 消抖：等10ms再读一次
		key_up = 0;            // 标记"本次按下已处理"，防止按住不动时反复触发
		if(WK_UP==1) return 1; // 10ms后还是高电平 → 确认真的按下，返回1
	}
	else if(WK_UP==0) key_up=1;// 引脚回到低电平（松开了）→ 复位标志
	return 0;// 无按键按下，返回0
}
