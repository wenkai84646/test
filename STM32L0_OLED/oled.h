#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

/* ============================================================
 * 0.96 寸 OLED 驱动（SSD1315，I2C 接口）
 * 引脚：PB10 = SCL，PB11 = SDA（软件模拟 I2C，见 oled.c）
 * ============================================================ */

/* I2C 从机地址：市面上 0.96 寸模块绝大多数是 0x3C。
 * 如果屏幕点不亮，检查模块背面电阻，是 0x3D 就把这里改成 0x3D */
#define OLED_ADDR   0x3C

#define OLED_W      128     /* 屏幕宽（像素） */
#define OLED_H      64      /* 屏幕高（像素） */

void oled_init(void);                            /* 初始化屏幕 */
void oled_clear(void);                           /* 清空显存（不刷屏） */
void oled_refresh(void);                         /* 把显存整体刷到屏幕 */
void oled_show_char16(int x, int y, const uint8_t *glyph);
                                                 /* 在(x,y)画一个16x16汉字 */

#endif
