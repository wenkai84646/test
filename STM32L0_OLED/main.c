/* ============================================================
 * STM32L051C8T6 + 0.96 寸 OLED（SSD1315，I2C）
 * 功能：在屏幕中央显示"你好"
 *
 * 接线：
 *   OLED SCL  ->  PB10
 *   OLED SDA  ->  PB11
 *   OLED VCC  ->  3.3V
 *   OLED GND  ->  GND
 *
 * 下载：ST-Link 接 SWD（SWDIO=PA13, SWCLK=PA14, 3.3V, GND）
 * ============================================================ */

#include "stm32l051xx.h"
#include "oled.h"
#include "font16.h"

/* ---------- 时钟初始化 ----------
 * 芯片复位后默认用内部 MSI 时钟（约 2.1MHz），
 * 这里切到内部 HSI 16MHz，延时函数按 16MHz 计算才准确 */
static void clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;                    /* 打开 HSI16 */
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) { }  /* 等它稳定 */

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;  /* 切换系统时钟 */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { } /* 等切换完成 */
}

/* ---------- GPIO 初始化 ----------
 * PB10、PB11 配置成推挽输出（软件模拟 I2C 只需要普通 GPIO，
 * 不需要配复用功能，所以怎么配都不会错） */
static void gpio_init(void)
{
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;          /* 打开 GPIOB 的时钟 */

    GPIOB->MODER  &= ~(0xFu << (2 * 10));       /* 清 PB10、PB11 的模式位 */
    GPIOB->MODER  |= (0x1u << (2 * 10)) |       /* PB10 = 通用输出 */
                     (0x1u << (2 * 11));        /* PB11 = 通用输出 */
    GPIOB->OTYPER &= ~((1u << 10) | (1u << 11));/* 推挽输出 */
    GPIOB->OSPEEDR |= (0x3u << (2 * 10)) |      /* 高速输出 */
                      (0x3u << (2 * 11));
    GPIOB->PUPDR  &= ~(0xFu << (2 * 10));       /* 不上拉不下拉（模块自带） */

    GPIOB->BSRR = (1u << 10) | (1u << 11);      /* 初始拉高 = I2C 总线空闲态 */
}

/* ---------- 主程序 ---------- */

int main(void)
{
    clock_init();
    gpio_init();

    oled_init();        /* 初始化屏幕（一连串配置命令） */
    oled_clear();       /* 清空显存 */

    /* "你好"两个字各 16x16，屏幕 128x64，居中：x=48 和 x=64，y=24 */
    oled_show_char16(48, 24, FONT16_NI);
    oled_show_char16(64, 24, FONT16_HAO);

    oled_refresh();     /* 把显存刷到屏幕，文字就出来了 */

    while (1) {
        /* 什么都不做，文字保持显示 */
    }
}
