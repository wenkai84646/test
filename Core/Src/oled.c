/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1306/SSD1315 0.96寸OLED显示屏驱动（I2C接口：PB10=SCL，PB11=SDA）
  *
  * 这个文件是自己写的OLED显示模块，主要功能：
  *   1. 初始化I2C2总线（PB10/PB11复用为I2C2）和OLED屏幕
  *   2. 提供画点、清屏、显示ASCII字符、显示汉字、刷新屏幕等函数
  *
  * 显示原理：
  *   SSD1306/SSD1315屏幕内部有128列x64行的显存，每个像素占1个bit（1=亮，0=灭），
  *   共128*64/8 = 1024字节。屏幕通过I2C总线接收这些数据并显示出来。
  *   本驱动在单片机RAM里维护一个同样大小的"显存副本"（oled_gram数组），
  *   所有画图操作先改副本，最后调用OLED_Refresh()把内容发给屏幕。
  *
  * 重要：oled_gram按"页"存放（[8][128] = 8页 x 128列），
  *   内存顺序和屏幕接收数据的顺序完全一致，刷新时直接按页发即可。
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "oled.h"       // 自己的头文件
#include "oledfont.h"   // 字模数据（ASCII 8x16 + 汉字16x16）
#include <string.h>     // memset函数

/* 汉字查找表：把GBK编码和字模对应起来
   以后想显示新的汉字：先在oledfont.h里加字模数据，再在这里加一条 */
typedef struct {
    uint16_t code;         // 汉字的GBK编码（2字节）
    const uint8_t *font;   // 对应的字模数据（16x16点阵，32字节）
} CN_CHAR_T;

static const CN_CHAR_T cn_table[] = {
    {0xC4E3, cn_ni},   // 你
    {0xBAC3, cn_hao},  // 好
};
#define CN_TABLE_SIZE (sizeof(cn_table) / sizeof(cn_table[0]))

/* 私有变量 ------------------------------------------------------------------*/
static I2C_HandleTypeDef hi2c_oled;    // OLED用的I2C句柄（I2C2），只在本文件内使用
static uint8_t oled_gram[8][128];      // 显存副本：8页 x 128列（每页8行，共64行）
static uint8_t oled_addr = OLED_ADDR;  // 屏幕的I2C地址：初始化时先试0x78，没应答会自动换成0x7A

/* 私有函数 ------------------------------------------------------------------*/

/* 发送一个命令给OLED
   I2C发送数据时第一个字节是"控制字节"：0x00表示后面是命令，0x40表示后面是显示数据 */
static void OLED_WriteCmd(uint8_t cmd)
{
    HAL_I2C_Mem_Write(&hi2c_oled, oled_addr, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

/* 发送一段显示数据给OLED（字模、显存内容等） */
static void OLED_WriteData(uint8_t *data, uint16_t len)
{
    HAL_I2C_Mem_Write(&hi2c_oled, oled_addr, 0x40, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

/* 公共函数 ------------------------------------------------------------------*/

/* 在显存副本上画一个点（color=1点亮，0熄灭）
   x和y用int16_t：允许负值（文字滚动时字符会有一半在屏幕外），越界的地方直接不画。
   这个是公共函数：main.c里的烟花效果也用它来画粒子 */
void OLED_DrawPoint(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;   // 越界保护：屏幕只有128列64行
    if (color) oled_gram[y / 8][x] |= (1 << (y % 8));    // 把这个bit置1 = 点亮
    else       oled_gram[y / 8][x] &= ~(1 << (y % 8));   // 把这个bit清0 = 熄灭
}

/* 在显存副本上显示一个8x16的ASCII字符 */
static void OLED_ShowChar(int16_t x, uint8_t y, uint8_t chr)
{
    int16_t i;
    uint8_t j;
    if (chr < 32 || chr > 126) return;      // 只支持可打印字符
    for (i = 0; i < 8; i++) {               // 8列
        for (j = 0; j < 16; j++) {          // 16行
            /* 字模一个字符占16字节：前8字节是上半部分，后8字节是下半部分，
               每字节对应一列，bit0在最上面一行 */
            uint8_t col_data = (j < 8) ? ascii_1608[chr - 32][i] : ascii_1608[chr - 32][i + 8];
            uint8_t bit = (j < 8) ? j : (j - 8);
            OLED_DrawPoint(x + i, y + j, (col_data >> bit) & 0x01);
        }
    }
}

/* 初始化：配置PB10/PB11为I2C2，探测并配置SSD1306/SSD1315屏幕
   返回值：找到的I2C地址（0x78或0x7A）；返回0 = 两个地址都没应答（屏幕没接好或模块不是I2C版） */
uint8_t OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t i;

    /* ① 打开GPIOB和I2C2的时钟（外设不打开时钟是不能工作的） */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    /* ② 配置PB10=SCL、PB11=SDA为I2C2复用功能
       I2C的引脚必须用"复用开漏"模式，靠上拉电阻把线拉高（OLED模块上自带4.7k上拉电阻） */
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;   // 选择引脚：PB10和PB11
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;            // 模式：复用开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;                // 内部上拉（和模块上的上拉电阻并联，没坏处）
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;      // 速度：高速
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;         // 复用功能：AF4 = I2C2
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);            // 写入GPIOB寄存器生效

    /* ③ 初始化I2C2：速度400kHz（快速模式）
       烟花动画每30ms要刷一整屏(1024字节)，100kHz刷一屏要100ms会卡成慢动作，
       所以提高到400kHz（SSD1306/SSD1315都支持快速模式）。
       如果屏幕显示乱码/花屏，说明你的模块走不了高速，把这里改回100000即可。
       F4的I2C是老式结构，分频系数不用自己算：HAL会根据ClockSpeed和PCLK1自动配置CCR等寄存器 */
    hi2c_oled.Instance = I2C2;
    hi2c_oled.Init.ClockSpeed = 400000;
    hi2c_oled.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c_oled.Init.OwnAddress1 = 0;                    // 自己是主机，这个参数用不上
    hi2c_oled.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c_oled.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c_oled.Init.OwnAddress2 = 0;
    hi2c_oled.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c_oled.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c_oled) != HAL_OK) {
        Error_Handler();
    }

    /* ④ 屏幕刚上电需要一点时间稳定 */
    HAL_Delay(100);

    /* ⑤ 探测屏幕地址：先试0x78，没应答再试0x7A
       HAL_I2C_IsDeviceReady()会往总线上发地址字节，看有没有设备应答（ACK）
       能收到ACK说明总线上有设备，收不到说明没接好/地址不对/模块不是I2C版 */
    if (HAL_I2C_IsDeviceReady(&hi2c_oled, OLED_ADDR, 3, 100) == HAL_OK) {
        oled_addr = OLED_ADDR;
    } else if (HAL_I2C_IsDeviceReady(&hi2c_oled, 0x7A, 3, 100) == HAL_OK) {
        oled_addr = 0x7A;
    } else {
        return 0;   // 两个地址都没有设备应答：返回0，让主函数用LED常亮提示错误
    }

    /* ⑥ 发送SSD1306/SSD1315初始化命令序列（每一条都是屏幕的配置） */
    static const uint8_t init_cmd[] = {
        0xAE,        // 关显示（配置完再打开）
        0xD5, 0x80,  // 设置显示时钟分频
        0xA8, 0x3F,  // 设置多路复用率：64行
        0xD3, 0x00,  // 设置显示偏移：0
        0x40,        // 设置显示起始行：第0行
        0x8D, 0x14,  // 打开内部电荷泵（OLED需要升压到7V左右才能发光）
        0x20, 0x02,  // 设置寻址模式：页面寻址（兼容性最好，山寨芯片也认这个模式）
        0xA1,        // 段重映射（决定左右方向，如果显示左右镜像了就改成0xA0）
        0xC8,        // COM扫描方向（决定上下方向，如果显示上下颠倒就改成0xC0）
        0xDA, 0x12,  // 设置COM引脚硬件配置
        0x81, 0xCF,  // 设置对比度（亮度）
        0xD9, 0xF1,  // 设置预充电周期
        0xDB, 0x40,  // 设置VCOMH电压
        0xA4,        // 显示内容跟随显存
        0xA6,        // 正常显示（不反色）
        0xAF,        // 开显示
    };
    for (i = 0; i < sizeof(init_cmd); i++) {
        OLED_WriteCmd(init_cmd[i]);
    }

    return oled_addr;   // 返回找到的地址
}

/* 清空显存（全屏熄灭）
   注意：只是改了内存里的副本，要调用OLED_Refresh()之后屏幕才真正变化 */
void OLED_Clear(void)
{
    memset(oled_gram, 0, sizeof(oled_gram));   // 1024字节全部写0 = 全灭
}

/* 在显存副本上显示一个16x16的汉字
   gbk_code是汉字的GBK编码，比如"你"=0xC4E3，"好"=0xBAC3 */
void OLED_ShowChinese(int16_t x, uint8_t y, uint16_t gbk_code)
{
    int16_t i;
    uint8_t j;
    const uint8_t *font = NULL;
    for (i = 0; i < CN_TABLE_SIZE; i++) {          // 在字库里查找这个汉字
        if (cn_table[i].code == gbk_code) {
            font = cn_table[i].font;
            break;
        }
    }
    if (font == NULL) return;                      // 字库里没有这个字就跳过
    for (i = 0; i < 16; i++) {                     // 16列
        for (j = 0; j < 8; j++) {                  // 上半部分8行
            OLED_DrawPoint(x + i, y + j, (font[i] >> j) & 0x01);
        }
        for (j = 0; j < 8; j++) {                  // 下半部分8行
            OLED_DrawPoint(x + i, y + 8 + j, (font[i + 16] >> j) & 0x01);
        }
    }
}

/* 显示字符串：ASCII字符用8x16字模，汉字用16x16字模
   GBK编码规则：一个汉字的编码是2个字节，每个字节都大于0x80，
   所以读到大于0x80的字节，就把它和下一个字节拼成一个汉字编码去查字库
   x允许是负值：文字滚动出屏幕左边时，负的部分会被DrawPoint自动丢弃 */
void OLED_ShowString(int16_t x, uint8_t y, char *str)
{
    while (*str) {
        if ((uint8_t)*str < 0x80) {                    // 小于0x80是ASCII字符
            OLED_ShowChar(x, y, (uint8_t)*str);
            x += 8;                                    // ASCII字符占8像素宽
            str += 1;
        } else {                                       // 大于等于0x80是汉字（占2个字节）
            uint16_t code = ((uint8_t)str[0] << 8) | (uint8_t)str[1];
            OLED_ShowChinese(x, y, code);
            x += 16;                                   // 汉字占16像素宽
            str += 2;
        }
        if (x >= 128) break;                           // 整个字符串都超出屏幕右边就不用画了
    }
}

/* 在显存副本上显示一个放大2倍的ASCII字符（8x16字模 → 16x32像素）
   原理：字模里每一个亮点都画成一个2x2的小方块，整个字就放大2倍 */
static void OLED_ShowChar2x(int16_t x, uint8_t y, uint8_t chr)
{
    int16_t i;
    uint8_t j;
    if (chr < 32 || chr > 126) return;      // 只支持可打印字符
    for (i = 0; i < 8; i++) {               // 字模8列
        for (j = 0; j < 16; j++) {          // 字模16行
            /* 字模一个字符占16字节：前8字节是上半部分，后8字节是下半部分 */
            uint8_t col_data = (j < 8) ? ascii_1608[chr - 32][i] : ascii_1608[chr - 32][i + 8];
            uint8_t bit = (j < 8) ? j : (j - 8);
            if ((col_data >> bit) & 0x01) {
                /* 一个亮点画成2x2的方块 = 放大2倍 */
                OLED_DrawPoint(x + i * 2,     y + j * 2,     1);
                OLED_DrawPoint(x + i * 2 + 1, y + j * 2,     1);
                OLED_DrawPoint(x + i * 2,     y + j * 2 + 1, 1);
                OLED_DrawPoint(x + i * 2 + 1, y + j * 2 + 1, 1);
            }
        }
    }
}

/* 显示2倍放大的字符串：ASCII字符画成16x32，汉字没有32x32字模所以还是16x16
   x允许是负值：文字滚动出屏幕左边时，负的部分会被DrawPoint自动丢弃 */
void OLED_ShowString2x(int16_t x, uint8_t y, char *str)
{
    while (*str) {
        if ((uint8_t)*str < 0x80) {                    // 小于0x80是ASCII字符
            OLED_ShowChar2x(x, y, (uint8_t)*str);
            x += 16;                                   // 放大后的ASCII字符占16像素宽
            str += 1;
        } else {                                       // 大于等于0x80是汉字（占2个字节）
            uint16_t code = ((uint8_t)str[0] << 8) | (uint8_t)str[1];
            OLED_ShowChinese(x, y, code);
            x += 16;                                   // 汉字没有放大，还是16像素宽
            str += 2;
        }
        if (x >= 128) break;                           // 整个字符串都超出屏幕右边就不用画了
    }
}

/* 把显存副本发送给屏幕（不调用这个函数，之前画的都不会显示）
   采用"逐页刷新"方式：每页8行，共8页，一页一页设置地址再发数据。
   这种方式不用0x21/0x22窗口命令，对山寨SSD1315/SSD1306芯片的兼容性最好。
   oled_gram是[8][128]（页x列），所以oled_gram[page]正好就是这一页的128个字节 */
void OLED_Refresh(void)
{
    uint8_t page;
    for (page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page);          // 设置页地址：第page页（每页8行，0xB0~0xB7）
        OLED_WriteCmd(0x00);                 // 列地址低4位：从第0列开始
        OLED_WriteCmd(0x10);                 // 列地址高4位：从第0列开始
        OLED_WriteData(oled_gram[page], 128);   // 发送这一页的128个字节（128列）
    }
}
