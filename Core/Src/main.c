/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"   // 主头文件，包含HAL库和芯片型号的定义
#include "tim.h"    // 定时器TIM13/TIM14的头文件
#include "gpio.h"   // GPIO引脚宏定义的头文件

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "key.h"    // 自己写的按键模块：提供4个按键扫描函数
#include "oled.h"   // 自己写的OLED显示模块：PB10=SCL，PB11=SDA
#include "math.h"   // 数学库：烟花效果的cosf/sinf用来计算粒子的速度分量
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FRAME_MS 30   // OLED画面帧间隔(毫秒)：烟花粒子每30ms更新一帧（约33帧/秒）
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t flow_enable = 1;    // 流水灯使能标志：1=流水灯正常运行，0=暂停（volatile防止编译器优化掉）
volatile uint16_t flow_delay = 200;  // 流水灯切换间隔(毫秒)：按PA0减50ms(加快)，按PE2加50ms(减慢)，范围50~1000

volatile uint8_t  song_playing = 0;  // 歌曲播放标志：0=停止，1=正在播放
volatile uint16_t note_index  = 0;   // 当前播放到第几个音符（数组下标）
volatile uint32_t note_tick   = 0;   // 下一个音符开始的时间点（HAL_GetTick()的计数值，单位ms）
volatile uint32_t led_tick    = 0;   // 流水灯上一次切换的时间点
volatile uint8_t  led_state   = 0;   // 流水灯当前状态：0=PF9亮，1=PF10亮

volatile uint8_t  oled_error  = 0;   // OLED错误标志：0=正常，1=初始化时没找到屏幕（两个LED会常亮提示）

volatile int16_t  scroll_x    = 128; // 滚动文字的X坐标：从屏幕右边缘外(128)开始，每次往左移动2像素
volatile uint32_t scroll_tick = 0;   // 滚动文字上一次移动的时间点
volatile uint32_t frame_tick  = 0;   // OLED画面上一帧的时间点（烟花动画每FRAME_MS毫秒更新一帧）
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*----------------------------------------------------------------------------*/
/* 音乐播放部分：用TIM13的PWM在PF8上输出不同频率的方波，让蜂鸣器发出不同音调   */
/*----------------------------------------------------------------------------*/

/* 音调频率表：下标是简谱数字（1=Do 2=Re 3=Mi 4=Fa 5=Sol 6=La 7=Si），单位Hz */
const uint16_t tone_freq[8] = {0, 523, 587, 659, 698, 784, 880, 988};

/* 一首《老鼠爱大米》副歌的曲谱：song_tone存音高（简谱数字），song_beat存每个音符的拍数 */
const uint8_t song_tone[] = {
    3,2,1, 3,2,1,           // 我爱你 爱着你
    1,3,2,1, 1,3,2,         // 就像老鼠爱大米
    3,1,3,6,5, 5,3,         // 不管有多少风雨
    6,6,5,5,6,5, 5,2,1,     // 我都会依然陪着你
    2,2,3, 2,1,2,           // 我想你 想着你
    2,3,2,1, 1,3,2,         // 不管有多么的苦
    3,1,3,6,5, 5,3,         // 只要能让你开心
    5,5,3,2,1,              // 我什么都愿意
    2,2,1,2, 2,1,1};        // 这样爱你
const uint8_t song_beat[] = {
    1,1,2, 1,1,2,           // 每句末尾的"你"拖2拍
    1,1,1,1, 1,1,2,
    1,1,1,1,1, 1,2,
    1,1,1,1,1,1, 1,1,2,
    1,1,1, 1,1,2,
    1,1,1,1, 1,1,2,
    1,1,1,1,1, 1,2,
    1,1,1,1,2,
    1,1,1,1, 1,1,3};        // 结尾"你"拖3拍收尾
#define SONG_NOTES (sizeof(song_tone)/sizeof(song_tone[0]))  // 音符总数

/* 用蜂鸣器演奏某一个音符：根据音高计算频率，驱动TIM13输出相应频率的方波 */
void Beep_Play_Tone(uint8_t tone)
{
    uint16_t arr = 1000000 / tone_freq[tone] - 1;   // 1MHz的计时时钟 / 频率 = 一个方波周期的计数值
    __HAL_TIM_SET_AUTORELOAD(&htim13, arr);         // 更新周期（因为开了预装载，写进预装载寄存器）
    __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, arr / 2);   // 50%占空比：半个周期高电平，半个周期低电平
    HAL_TIM_GenerateEvent(&htim13, TIM_EVENTSOURCE_UPDATE); // 生成更新事件，让新的周期和占空比立即生效
}

/* 演奏结束停止发声：占空比改成0，输出一直保持低电平，蜂鸣器就不响了 */
void Beep_Stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, 0);
    HAL_TIM_GenerateEvent(&htim13, TIM_EVENTSOURCE_UPDATE); // 生成更新事件，让占空比立即生效
}

/*----------------------------------------------------------------------------*/
/* 烟花效果：在OLED屏幕上用粒子动画模拟放烟花（和"zhy520"滚动文字同屏共存）  */
/* 原理：屏幕底部随机位置发射一个亮点（火箭），升到随机高度后"爆炸"成20多个   */
/* 粒子，每个粒子有随机方向和速度，受重力影响慢慢往下掉，寿命结束就熄灭。     */
/* 粒子坐标用float小数（每次只移动零点几像素），动画才平滑不像马赛克。        */
/*----------------------------------------------------------------------------*/

#define FW_SLOTS 3      // 最多同时放3颗烟花
#define FW_PMAX  28     // 一颗烟花最多28个粒子

/* 一个烟花粒子：坐标、速度、寿命 */
typedef struct {
    float x, y;        // 粒子坐标（小数：正x=向右，正y=向下）
    float vx, vy;      // 粒子速度：每帧移动的像素数
    uint8_t life;      // 剩余寿命（帧数）：减到0粒子就熄灭
} FW_Particle;

/* 一颗烟花：先上升（火箭阶段），再爆炸（粒子阶段） */
typedef struct {
    uint8_t state;             // 状态：0=空闲，1=火箭上升，2=粒子爆炸
    float x, y;                // 火箭当前坐标（也是爆炸中心坐标）
    uint8_t burst_y;           // 预定爆炸高度
    FW_Particle p[FW_PMAX];    // 爆炸后的粒子数组
    uint8_t pnum;              // 粒子个数
} Firework;

static Firework fw[FW_SLOTS];    // 3个烟花槽位（全局变量默认全0=全部空闲）
static uint32_t fw_seed = 1;     // 随机数种子

/* 11伪随机数生成器（线性同余法）：单片机常用的简单随机数函数，返回0~32767 */
static uint32_t FW_Rand(void)
{
    fw_seed = fw_seed * 1103515245 + 12345;
    return (fw_seed >> 16) & 0x7FFF;
}

/* 取一个[min,max]之间的随机整数 */
static int16_t FW_RandRange(int16_t min, int16_t max)
{
    return min + (int16_t)(FW_Rand() % (max - min + 1));
}

/* 取一个[min,max]之间的随机小数（粒子速度随机用） */
static float FW_RandF(float min, float max)
{
    return min + (max - min) * (FW_Rand() / 32768.0f);
}

/* 爱心特效函数的前置声明：Firework_Update在爆炸时会调用Heart_Spawn，
   而Heart_Spawn的定义在文件后面的爱心特效部分 */
static void Heart_Spawn(int16_t bx, int16_t by);

/* 初始化烟花：随机种子取当前时间，并立刻发射第一发火箭（开机马上能看到效果） */
void Firework_Init(void)
{
    fw_seed = HAL_GetTick();
    fw[0].state = 1;
    fw[0].x = 64;          // 第一发从屏幕中间发射
    fw[0].y = 63;          // 从屏幕底部起飞
    fw[0].burst_y = 24;    // 在屏幕中间偏上的位置爆炸
}

/* 更新烟花动画一帧（主循环每FRAME_MS毫秒调用一次）
   返回值：1=画面内容有变化需要刷新屏幕，0=什么都没动不用刷新 */
uint8_t Firework_Update(void)
{
    uint8_t s, i;
    uint8_t active = 0;
    for (s = 0; s < FW_SLOTS; s++) {
        if (fw[s].state == 0) {
            /* 空闲：每帧有1/60的概率发射新火箭（平均约1.8秒发射一发） */
            if (FW_Rand() % 60 == 0) {
                fw[s].state = 1;
                fw[s].x = (float)FW_RandRange(15, 112);   // 随机发射位置（避开屏幕左右边缘）
                fw[s].y = 63;                             // 从屏幕底部起飞
                fw[s].burst_y = (uint8_t)FW_RandRange(8, 32);  // 随机爆炸高度
                active = 1;
            }
        } else if (fw[s].state == 1) {
            /* 火箭上升阶段：亮点每帧往上飞2.2像素 */
            fw[s].y -= 2.2f;
            if (fw[s].y <= fw[s].burst_y) {
                /* 到达爆炸高度 → 炸开：生成一圈随机方向和速度的粒子 */
                fw[s].state = 2;
                fw[s].pnum = (uint8_t)FW_RandRange(18, FW_PMAX);   // 随机粒子个数
                for (i = 0; i < fw[s].pnum; i++) {
                    float ang = 6.2832f * FW_Rand() / 32768.0f;    // 随机角度：0~2π（整个圆都炸开）
                    float spd = FW_RandF(0.6f, 2.0f);              // 随机初速度（像素/帧）
                    fw[s].p[i].x = fw[s].x;
                    fw[s].p[i].y = fw[s].y;
                    fw[s].p[i].vx = spd * cosf(ang);   // 速度分解：水平分量
                    fw[s].p[i].vy = spd * sinf(ang);   // 速度分解：竖直分量
                    fw[s].p[i].life = (uint8_t)FW_RandRange(25, 45);   // 随机寿命（帧数）
                }
                /* 偶尔（约20%概率）在爆炸点附近冒出一颗爱心 */
                if (FW_Rand() % 100 < 20) {
                    Heart_Spawn((int16_t)fw[s].x, (int16_t)fw[s].y);
                }
            }
            active = 1;
        } else {
            /* 爆炸阶段：更新每个粒子的位置（重力让它往下掉）和寿命 */
            uint8_t alive = 0;
            for (i = 0; i < fw[s].pnum; i++) {
                if (fw[s].p[i].life == 0) continue;    // 已经熄灭的粒子跳过
                fw[s].p[i].life--;                     // 寿命减1帧
                fw[s].p[i].x += fw[s].p[i].vx;         // 按速度移动
                fw[s].p[i].y += fw[s].p[i].vy;
                fw[s].p[i].vy += 0.04f;                // 重力：竖直速度每帧增加，粒子慢慢往下掉
                if (fw[s].p[i].life > 0) alive = 1;    // 还有没熄灭的粒子
            }
            if (!alive) {
                fw[s].state = 0;   // 所有粒子都熄灭：烟花放完，槽位空闲
                active = 1;        // 这一帧也要刷新：把最后熄灭的粒子擦干净
            } else {
                active = 1;
            }
        }
    }
    return active;   // 只要有火箭或粒子在动，画面就有变化
}

/* 把烟花画进OLED显存（画完还要调OLED_Refresh()才真正显示）
   画粒子时在速度反方向多画一个点形成"拖尾"，粒子看起来像拖着尾巴的流星 */
void Firework_Draw(void)
{
    uint8_t s, i;
    for (s = 0; s < FW_SLOTS; s++) {
        if (fw[s].state == 1) {
            OLED_DrawPoint((int16_t)fw[s].x, (int16_t)fw[s].y, 1);   // 火箭：一个亮点
        } else if (fw[s].state == 2) {
            for (i = 0; i < fw[s].pnum; i++) {
                if (fw[s].p[i].life == 0) continue;
                OLED_DrawPoint((int16_t)fw[s].p[i].x, (int16_t)fw[s].p[i].y, 1);    // 粒子本体
                OLED_DrawPoint((int16_t)(fw[s].p[i].x - fw[s].p[i].vx),             // 尾巴点
                               (int16_t)(fw[s].p[i].y - fw[s].p[i].vy), 1);
            }
        }
    }
}
/*----------------------------------------------------------------------------*/
/* 爱心特效：烟花爆炸时偶尔（约20%概率）在爆炸点附近冒出一颗会"心跳"的爱心     */
/* 原理：爱心是一张16x10的点阵图，平时2倍放大成32x20像素；每8帧的最后2帧       */
/* 缩回1倍再放大，看起来像心跳一收一缩；寿命结束后闪烁几下消失。               */
/*----------------------------------------------------------------------------*/

#define HEART_W 16      // 爱心点阵宽度（16像素）
#define HEART_H 10      // 爱心点阵高度（10像素）

/* 爱心点阵：一行16bit，1=亮、0=灭（左边最高位、右边最低位） */
static const uint16_t heart_bmp[HEART_H] = {
    0x6006,  // ..XX........XX..
    0xF81F,  // XXXXX....XXXXX
    0xFFFF,  // XXXXXXXXXXXXXX
    0xFFFF,  // XXXXXXXXXXXXXX
    0x7FFE,  // .XXXXXXXXXXXX.
    0x3FFC,  // ..XXXXXXXXXX..
    0x1FF8,  // ...XXXXXXXX...
    0x0FF0,  // ....XXXXXX....
    0x07E0,  // .....XXXX.....
    0x0180,  // ......XX......
};

/* 一颗爱心：位置 + 剩余寿命（帧数），life=0表示当前没有爱心 */
typedef struct {
    int16_t x, y;    // 爱心左上角坐标
    uint8_t life;    // 剩余寿命：每帧减1，减到0爱心消失
} HeartFX;

static HeartFX heart = {0, 0, 0};   // 初始状态：没有爱心

/* 生成爱心：在爆炸点附近随机偏移一个位置 */
static void Heart_Spawn(int16_t bx, int16_t by)
{
    heart.x = bx + FW_RandRange(-22, 22);   // 水平随机偏移±22像素
    heart.y = by + FW_RandRange(-14, 14);   // 垂直随机偏移±14像素
    if (heart.x < 0) heart.x = 0;                       // 限制在屏幕内
    if (heart.x > 128 - HEART_W * 2) heart.x = 128 - HEART_W * 2;  // 2倍放大后宽32
    if (heart.y < 0) heart.y = 0;
    if (heart.y > 64 - HEART_H * 2) heart.y = 64 - HEART_H * 2;    // 2倍放大后高20
    heart.life = 48;                        // 寿命48帧，约1.5秒（48×30ms）
}

/* 爱心更新一帧：寿命减1
   返回值：1=爱心还在变化（需要刷新屏幕），0=没有爱心或已经消失 */
static uint8_t Heart_Update(void)
{
    if (heart.life == 0) return 0;   // 当前没有爱心
    heart.life--;                    // 寿命减1帧
    return 1;
}

/* 把爱心画进OLED显存：
   平时2倍放大（32x20像素），每8帧的最后2帧缩成1倍，像心跳一样一收一缩；
   最后6帧改成1倍大小一闪一闪（隔一帧画一次），闪完就消失 */
static void Heart_Draw(void)
{
    uint8_t r, c, scale;
    if (heart.life == 0) return;             // 当前没有爱心
    if (heart.life < 6) {                    // 最后6帧：缩小并闪烁
        if (heart.life & 1) return;          // 奇数帧不画=闪烁效果
        scale = 1;
    } else if (heart.life % 8 >= 6) {        // 每8帧的最后2帧收缩一下
        scale = 1;
    } else {
        scale = 2;                           // 平时2倍放大
    }
    for (r = 0; r < HEART_H; r++) {
        for (c = 0; c < HEART_W; c++) {
            if (!(heart_bmp[r] & (0x8000 >> c))) continue;   // 这个点是熄灭的
            OLED_DrawPoint(heart.x + c * scale, heart.y + r * scale, 1);
            if (scale == 2) {                // 一个点放大成2x2的方块
                OLED_DrawPoint(heart.x + c * scale + 1, heart.y + r * scale, 1);
                OLED_DrawPoint(heart.x + c * scale,     heart.y + r * scale + 1, 1);
                OLED_DrawPoint(heart.x + c * scale + 1, heart.y + r * scale + 1, 1);
            }
        }
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();               // ① 复位各个外设、初始化Flash和SysTick（HAL_Delay和HAL_GetTick的时钟基础）

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* Configure the system clock */
  SystemClock_Config();     // ② 配置系统时钟到168MHz（芯片跑满全速）

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */
  /* Initialize all configured peripherals */
  MX_GPIO_Init();           // ③ 初始化GPIO（LED/按键等引脚都已经配置成对应模式）
  MX_TIM14_Init();          // ④ 初始化定时器TIM14（之前学习用的，现在没用了，保留着）
  MX_TIM13_Init();          // ⑤ 初始化定时器TIM13（PWM输出，用来驱动蜂鸣器发声）

  /* USER CODE BEGIN 2 */
  /* 启动TIM13的PWM输出（初始占空比为0，所以不会响；播放音符时再改变频率） */
  HAL_TIM_PWM_Start(&htim13, TIM_CHANNEL_1);

  /* OLED初始化：PB10=SCL，PB11=SDA，走I2C协议
     OLED_Init()会先试地址0x78、再试0x7A；返回0表示两个地址都没应答 */
  if (OLED_Init() != 0) {
    OLED_Clear();        // 先清屏；"zhy520"滚动文字和烟花动画都由主循环负责画
    OLED_Refresh();
    Firework_Init();     // 初始化烟花：立刻发射第一发火箭（开机马上能看到效果）
  } else {
    /* 没找到屏幕：两个LED常亮作为报警（正常流水灯是一亮一灭，两个常亮=I2C没通） */
    oled_error = 1;
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)   // 主循环：单片机的主程序永远在这个循环里运行，叫"永远执行"
  {
        /* ============ 流水灯（非阻塞方式：每隔flow_delay毫秒切换一次） ============ */
        /* 用HAL_GetTick()计时，不用HAL_Delay()傻等，这样音乐和按键扫描都不受影响 */
        if (flow_enable && !oled_error) {   // OLED初始化失败时（oled_error=1）两个LED常亮报警，不跑流水灯
            if (HAL_GetTick() - led_tick >= flow_delay) {   // 距离上次切换已经过去flow_delay毫秒
                led_tick = HAL_GetTick();                   // 记录这次切换的时间点
                led_state = !led_state;                     // 切换状态：0变1、1变0
                HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, led_state ? GPIO_PIN_RESET : GPIO_PIN_SET);  // 状态=1时PF9亮
                HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET); // 状态=1时PF10亮
            }
        }

        /* ============ OLED画面渲染：文字滚动 + 烟花效果（非阻塞） ============ */
        /* 烟花粒子要快速移动才像烟花，所以画面每FRAME_MS毫秒更新一帧（约33帧/秒）；
           文字滚动每flow_delay毫秒移动2像素（和流水灯共用节拍，速度联动）。
           画面没变化就不刷新屏幕，把时间省给主循环干别的事 */
        if (!oled_error) {
            if (HAL_GetTick() - frame_tick >= FRAME_MS) {
                frame_tick = HAL_GetTick();      // 记录这一帧的时间点
                uint8_t frame_changed = 0;       // 这一帧画面有没有变化（没变化就不用刷新屏幕）
                /* ① 文字滚动：每flow_delay毫秒往左移动2像素 */
                if (HAL_GetTick() - scroll_tick >= flow_delay) {
                    scroll_tick = HAL_GetTick();
                    scroll_x -= 2;                       // 往左移动2像素
                    if (scroll_x <= -96) scroll_x = 128; // 整个"zhy520"(6个字符x16像素=96宽)都移出左边就从右边回来
                    frame_changed = 1;
                }
                /* ② 烟花动画：更新粒子物理（自动发射新火箭、爆炸、重力下落、寿命熄灭） */
                if (Firework_Update()) frame_changed = 1;
                /* ③ 爱心特效：寿命每帧减1（烟花爆炸时随机冒出的爱心跳1秒多后消失） */
                if (Heart_Update()) frame_changed = 1;
                /* ④ 组装这一帧画面并刷新：先清屏，再画滚动文字，再画烟花粒子，最后画爱心（爱心盖在最上面） */
                if (frame_changed) {
                    OLED_Clear();
                    OLED_ShowString2x(scroll_x, 16, "zhy520");   // 文字2倍放大(32像素高)垂直居中：16=(64-32)/2
                    Firework_Draw();
                    Heart_Draw();
                    OLED_Refresh();
                }
            }
        }

        /* ============ 音乐播放（非阻塞方式：一个音符播完自动播下一个） ============ */
        if (song_playing) {
            if (HAL_GetTick() >= note_tick) {               // 当前音符的时长到了
                if (note_index >= SONG_NOTES) {             // 曲谱播完了
                    song_playing = 0;
                    Beep_Stop();                            // 把蜂鸣器关掉
                } else {
                    Beep_Play_Tone(song_tone[note_index]);  // 设置当前音符的频率，蜂鸣器马上发出声音
                    /* 音符时长 = 拍数 × flow_delay ÷ 2
                       ÷2让音乐比流水灯快一倍（"快一点"）；想再快就把2改大
                       流水灯越快(flow_delay越小) → 每拍越短 → 音乐越快，反之音乐越慢 */
                    note_tick = HAL_GetTick() + (uint32_t)song_beat[note_index] * flow_delay / 2;
                    note_index++;                           // 指向下一个音符
                }
            }
        }

        /* ============ 按键扫描：检测KEY0（PE4）暂停/恢复流水灯 ============ */
        Key0_flag = KEY_Scan(0);   // 调用按键扫描函数：检测到按下返回1，没有按下返回0（内部有10ms消抖）
        if (Key0_flag == 1) {
            Key0_flag = 0;                 // 清除标志，防止同一次按压被重复处理
            flow_enable = !flow_enable;    // 使能标志取反：按一下暂停，再按一下恢复
            if (!flow_enable) {
                // 暂停流水灯时，把两个LED都熄灭
                HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET);
            }
        }

        /* ============ 按键扫描：检测KEY1（PE3）播放/停止音乐《老鼠爱大米》 ============ */
        Key1_flag = KEY1_Scan(0);   // 检测到按下返回1，没有按下返回0（内部自带消抖）
        if (Key1_flag == 1) {
            Key1_flag = 0;                 // 清除标志
            song_playing = !song_playing;  // 播放标志取反：按一下开始唱歌，再按一下停止
            if (song_playing) {
                note_index = 0;            // 从头开始播放
                note_tick = HAL_GetTick(); // 立即播放第一个音符
            } else {
                Beep_Stop();               // 停止时把蜂鸣器关掉
            }
        }

        /* ============ 按键扫描：检测WK_UP（PA0）加快流水灯速度 ============ */
        WKUP_flag = WKUP_Scan(0);   // WK_UP检测到按下返回1，注意：这个按键按下是高电平，和其他按键相反
        if (WKUP_flag == 1) {
            WKUP_flag = 0;                 // 清除标志，防止同一次按压被重复处理
            if (flow_delay > 50) {         // 下限保护：最短不能小于50ms，防止减到0
                flow_delay -= 50;          // 间隔减50ms → 流水灯更快，音乐也跟着变快
            }
        }

        /* ============ 按键扫描：检测KEY2（PE2）减慢流水灯速度 ============ */
        Key2_flag = KEY2_Scan(0);   // KEY2检测到按下返回1（注意是低电平）
        if (Key2_flag == 1) {
            Key2_flag = 0;                 // 清除标志，防止同一次按压被重复处理
            if (flow_delay < 1000) {       // 上限保护：最长不能超过1000ms，防止加到太大
                flow_delay += 50;          // 间隔加50ms → 流水灯变慢，音乐也跟着变慢
            }
        }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
// 配置系统时钟到168MHz（CubeMX自动生成，一般不用改）
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();   // 关闭全局中断
  while (1)          // 死循环停在这里：卡住=程序初始化出错了，调试时从这里开始找
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error source line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
