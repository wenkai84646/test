#ifndef __OLED_H
#define __OLED_H

#include "main.h"   // 为了用到HAL库的类型定义

/* OLED的I2C从机地址：0x3C左移1位 = 0x78
   I2C地址是7位的，传输时第一个字节的高7位是地址、最低位是读写标志。
   有的模块背面DC引脚接法不同，地址会是0x7A——OLED_Init()会先试0x78，
   没应答再自动试0x7A，不用手动改这个宏 */
#define OLED_ADDR 0x78

uint8_t OLED_Init(void);   // 初始化I2C2和OLED屏幕；返回找到的地址(0x78或0x7A)，返回0=总线上没找到屏幕（没接好或模块不是I2C版）
void OLED_Clear(void);                                  // 清空显存（全屏熄灭）
void OLED_DrawPoint(int16_t x, int16_t y, uint8_t color);   // 在显存副本上画一个点：color=1点亮、0熄灭，越界自动忽略（烟花粒子动画也用它）
void OLED_ShowString(int16_t x, uint8_t y, char *str);  // 在显存里写字符串：ASCII用8x16字模，汉字用16x16字模（x允许负值，用于文字滚出屏幕左边）
void OLED_ShowString2x(int16_t x, uint8_t y, char *str); // 写2倍放大的字符串：ASCII画成16x32（每个亮点变2x2方块），汉字还是16x16
void OLED_ShowChinese(int16_t x, uint8_t y, uint16_t gbk_code);   // 在显存里写一个16x16汉字
void OLED_Refresh(void);                                // 把显存副本通过I2C发送到屏幕显示

#endif /* __OLED_H */
