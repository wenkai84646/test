/* ============================================================
 * 0.96 寸 OLED（SSD1315）驱动 —— 软件模拟 I2C 版
 *
 * 为什么用软件模拟 I2C（GPIO 翻转电平模拟时序）而不是硬件 I2C？
 *   1. 不用配置复用功能、时钟、时序寄存器，出错点少，适合入门
 *   2. OLED 刷新 1KB 画面只需要几十毫秒，模拟 I2C 速度完全够用
 *
 * 注意：本文件假设系统时钟已经切到 HSI 16MHz（main.c 的 clock_init）
 * ============================================================ */

#include "stm32l051xx.h"
#include "oled.h"

#define SCL_PIN     10      /* PB10 */
#define SDA_PIN     11      /* PB11 */

/* ---------- 微秒级延时（SysTick 轮询，16MHz 下 1us = 16 个计数） ---------- */

static void delay_us(uint32_t us)
{
    if (us == 0) return;
    SysTick->LOAD = us * 16 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) { }
    SysTick->CTRL = 0;
}

static void delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000);
}

/* ---------- I2C 底层：用 GPIO 模拟 SCL / SDA 时序 ---------- */

static void scl_high(void) { GPIOB->BSRR = (1u << SCL_PIN); }
static void scl_low(void)  { GPIOB->BSRR = (1u << (SCL_PIN + 16)); }
static void sda_high(void) { GPIOB->BSRR = (1u << SDA_PIN); }
static void sda_low(void)  { GPIOB->BSRR = (1u << (SDA_PIN + 16)); }

/* 起始信号：SCL 高电平时 SDA 从高拉低 */
static void i2c_start(void)
{
    sda_high(); delay_us(2);
    scl_high(); delay_us(2);
    sda_low();  delay_us(2);
    scl_low();  delay_us(2);
}

/* 停止信号：SCL 高电平时 SDA 从低拉高 */
static void i2c_stop(void)
{
    sda_low();  delay_us(2);
    scl_high(); delay_us(2);
    sda_high(); delay_us(2);
}

/* 写一个字节（高位在前），第 9 个时钟是应答位。
 * 这里不检查应答（OLED 模块一般都会正常应答），返回 1 表示发完 */
static void i2c_write_byte(uint8_t b)
{
    uint32_t i;
    for (i = 0; i < 8; i++) {
        if (b & 0x80) sda_high();
        else          sda_low();
        delay_us(2);
        scl_high(); delay_us(2);
        scl_low();  delay_us(2);
        b <<= 1;
    }
    sda_high();         /* 释放数据线，让从机拉低表示应答 */
    delay_us(2);
    scl_high(); delay_us(2);
    scl_low();  delay_us(2);
}

/* ---------- SSD1315 命令 / 数据接口 ---------- */

/* 控制字节：0x00 后面跟命令，0x40 后面跟数据 */
static void oled_write_cmd(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(OLED_ADDR << 1);
    i2c_write_byte(0x00);
    i2c_write_byte(cmd);
    i2c_stop();
}

static void oled_write_data(uint8_t dat)
{
    i2c_start();
    i2c_write_byte(OLED_ADDR << 1);
    i2c_write_byte(0x40);
    i2c_write_byte(dat);
    i2c_stop();
}

/* ---------- 显存：8 页 x 128 列，共 1024 字节 ----------
 * OLED 一列 8 个像素对应 1 个字节（bit0 在最上面） */
static uint8_t fb[OLED_H / 8][OLED_W];

/* ---------- 初始化序列（SSD1315 与 SSD1306 命令兼容） ---------- */

void oled_init(void)
{
    delay_ms(20);                       /* 等模块上电稳定 */

    oled_write_cmd(0xAE);               /* 关显示 */

    oled_write_cmd(0x20); oled_write_cmd(0x00);   /* 水平寻址模式 */
    oled_write_cmd(0x40);               /* 起始行地址 0 */
    oled_write_cmd(0xA1);               /* 段重映射（左右不颠倒） */
    oled_write_cmd(0xC8);               /* COM 扫描方向（上下不颠倒） */
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);   /* 复用率 1/64 */
    oled_write_cmd(0xD3); oled_write_cmd(0x00);   /* 显示偏移 0 */
    oled_write_cmd(0xD5); oled_write_cmd(0x80);   /* 显示时钟分频 */
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);   /* 预充电周期 */
    oled_write_cmd(0xDA); oled_write_cmd(0x12);   /* COM 引脚硬件配置 */
    oled_write_cmd(0xDB); oled_write_cmd(0x30);   /* VCOMH 电压 */
    oled_write_cmd(0x81); oled_write_cmd(0xCF);   /* 对比度 207 */

    oled_write_cmd(0xA4);               /* 显示跟随 RAM 内容 */
    oled_write_cmd(0xA6);               /* 正常显示（不反白） */
    oled_write_cmd(0xAF);               /* 开显示 */
}

/* ---------- 基本操作 ---------- */

void oled_clear(void)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)fb;
    for (i = 0; i < sizeof(fb); i++) p[i] = 0x00;
}

/* 在 (x, y) 处画一个 16x16 点阵汉字（glyph 是 32 字节点阵数据） */
void oled_show_char16(int x, int y, const uint8_t *glyph)
{
    int row, col;
    for (row = 0; row < 16; row++) {
        uint16_t line = (uint16_t)(glyph[row * 2] << 8) | glyph[row * 2 + 1];
        for (col = 0; col < 16; col++) {
            if (line & (0x8000u >> col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < OLED_W && py >= 0 && py < OLED_H)
                    fb[py >> 3][px] |= (uint8_t)(1u << (py & 7));
            }
        }
    }
}

/* 把整块显存一次性发到屏幕（水平寻址模式下地址自动递增） */
void oled_refresh(void)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)fb;

    oled_write_cmd(0x21); oled_write_cmd(0); oled_write_cmd(OLED_W - 1);
    oled_write_cmd(0x22); oled_write_cmd(0); oled_write_cmd(OLED_H / 8 - 1);

    i2c_start();
    i2c_write_byte(OLED_ADDR << 1);
    i2c_write_byte(0x40);
    for (i = 0; i < sizeof(fb); i++) {
        i2c_write_byte(p[i]);
    }
    i2c_stop();
}
