/* 烟花表演 + 点亮 "郑老师教师节快乐哦QAQ"
 *
 * 编译：gcc teacherday.c -o teacherday.exe
 * 运行：在真正的终端里执行 teacherday.exe（不要在 IDE 输出面板里运行）
 * 效果：先是一阵烟花秀，然后两行文字被逐渐点亮并闪烁
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include "font_js.h"        /* 点阵字体数据（genfont.ps1 生成） */

#define W   120             /* 画面宽(列) */
#define H   27              /* 画面高(行) */
#define MAX_PARTS   900     /* 粒子上限 */
#define MAX_ROCKETS 12      /* 同时升空的火箭上限 */
#define PHASE2      180     /* 第几帧开始点亮文字 */

/* 字符表：0空格 1星 2点 3竖线 4实心方块 */
const char *CH[] = { " ", "*", ".", "|", "█" };
#define BLOCK 4

/* 烟花颜色表（白红黄绿青紫蓝） */
const int BRIGHT[] = { 15, 12, 14, 10, 11, 13, 9 };
#define BRIGHT_N 7

/* 文字排版：两行，每行 6 个字，每字 18 列 x 11 行 */
#define CHARS   12
#define LINE_N  6
#define CW      18              /* 每字宽 */
#define CHH     11              /* 每字高 */
#define STRIDE  20              /* 字距 = 18 + 2 间距 */
#define LINE_W  (LINE_N * STRIDE - 2)
#define TEXT_X0 ((W - LINE_W) / 2)
#define TEXT_Y0 1
#define LINE_GAP 2

/* ---------- 数据结构 ---------- */

typedef struct {            /* 烟花粒子 */
    float x, y;             /* 坐标（用小数，移动更平滑） */
    float vx, vy;           /* 速度 */
    int life, maxLife;      /* 剩余寿命 / 总寿命（单位：帧） */
    int color;
} Particle;

typedef struct {            /* 升空的火箭 */
    float x, y;
    float vy;               /* 上升速度（负数向上） */
    float targetY;          /* 爆炸高度 */
    int color;
} Rocket;

Particle parts[MAX_PARTS];
int partCount = 0;
Rocket  rockets[MAX_ROCKETS];
int rocketCount = 0;

unsigned char grid[H][W], prev[H][W];   /* 当前帧 / 上一帧画面 */
int gridCol[H][W], prevCol[H][W];       /* 对应的颜色 */
int t = 0;                              /* 帧计数器 */

/* 文字像素列表：坐标、颜色、点亮时刻 */
#define TCELL_MAX 1800
int tcx[TCELL_MAX], tcy[TCELL_MAX], tcol[TCELL_MAX], tdelay[TCELL_MAX];
int tcellCount = 0;

/* ---------- 控制台基础操作 ---------- */

void gotoxy(int x, int y)
{
    COORD pos = { x, y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

void setColor(int color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void hideCursor(void)
{
    CONSOLE_CURSOR_INFO ci;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(h, &ci);
}

void showCursor(void)
{
    CONSOLE_CURSOR_INFO ci;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(h, &ci);
}

/* ---------- 粒子和火箭 ---------- */

/* 添加一个粒子 */
void spawnParticle(float x, float y, float vx, float vy, int life, int color)
{
    Particle *p;
    if (partCount >= MAX_PARTS) return;
    p = &parts[partCount++];
    p->x = x; p->y = y;
    p->vx = vx; p->vy = vy;
    p->life = p->maxLife = life;
    p->color = color;
}

/* 火箭爆炸：向四周喷出大量粒子 */
void explode(Rocket *r)
{
    int i, n = 80 + rand() % 60;                    /* 80~140 个粒子 */
    for (i = 0; i < n; i++) {
        float ang = (float)(rand() % 6283) / 1000.0f;   /* 0 ~ 2π 随机角度 */
        float spd = 0.3f + (float)(rand() % 140) / 100.0f; /* 0.3 ~ 1.7 随机速度 */
        spawnParticle(r->x, r->y,
                      (float)cos(ang) * spd, (float)sin(ang) * spd,
                      25 + rand() % 45, r->color);
    }
    /* 中心补几个白色闪光 */
    for (i = 0; i < 12; i++)
        spawnParticle(r->x, r->y,
                      (float)(rand() % 60 - 30) / 100.0f,
                      (float)(rand() % 60 - 30) / 100.0f,
                      6 + rand() % 6, 15);
}

/* 从屏幕底部发射一枚火箭 */
void launchRocket(void)
{
    Rocket *r;
    if (rocketCount >= MAX_ROCKETS) return;
    r = &rockets[rocketCount++];
    r->x = 5.0f + rand() % (W - 10);
    r->y = (float)(H - 2);
    r->vy = -(0.5f + (float)(rand() % 40) / 100.0f);
    r->targetY = 4.0f + rand() % 14;
    r->color = BRIGHT[rand() % BRIGHT_N];
}

/* 更新所有火箭：上升 + 拖尾 + 到达高度后爆炸 */
void updateRockets(void)
{
    int i;
    for (i = 0; i < rocketCount; i++) {
        Rocket *r = &rockets[i];
        /* 拖尾：在当前位置留下一粒很快熄灭的小粒子 */
        spawnParticle(r->x, r->y, 0, 0, 4 + rand() % 4, r->color);
        r->y += r->vy;
        if (r->y <= r->targetY) {                   /* 到达爆炸高度 */
            explode(r);
            rockets[i] = rockets[--rocketCount];    /* 用最后一个顶替并删掉 */
            i--;
        }
    }
}

/* 更新所有粒子：重力 + 移动 + 寿命 */
void updateParticles(void)
{
    int i;
    for (i = 0; i < partCount; i++) {
        Particle *p = &parts[i];
        p->vy += 0.03f;                             /* 重力 */
        p->x += p->vx;
        p->y += p->vy;
        p->life--;
        /* 寿命耗尽或飞出屏幕就删除 */
        if (p->life <= 0 || p->x < 0 || p->x >= W || p->y >= H) {
            parts[i] = parts[--partCount];
            i--;
        }
    }
}

/* ---------- 绘制 ---------- */

/* 把粒子画进画面缓冲 */
void drawParticles(void)
{
    int i, x, y;
    for (i = 0; i < partCount; i++) {
        x = (int)parts[i].x;
        y = (int)parts[i].y;
        if (x < 0 || x >= W || y < 0 || y >= H) continue;
        /* 前半生画 '*'，后半生画 '.'（越飞越小） */
        grid[y][x] = (parts[i].life * 2 > parts[i].maxLife) ? 1 : 2;
        gridCol[y][x] = parts[i].color;
    }
}

/* 把火箭画进画面缓冲 */
void drawRockets(void)
{
    int i, x, y;
    for (i = 0; i < rocketCount; i++) {
        x = (int)rockets[i].x;
        y = (int)rockets[i].y;
        if (x >= 0 && x < W && y >= 0 && y < H) {
            grid[y][x] = 3;                         /* '|' */
            gridCol[y][x] = 15;
        }
    }
}

/* 点亮文字：到点的像素亮起来，已亮的偶尔换色闪烁 */
void drawText(void)
{
    int i;
    for (i = 0; i < tcellCount; i++) {
        if (t < tdelay[i]) continue;                /* 还没到点亮时刻 */
        if (rand() % 150 == 0)                      /* 偶尔变色，产生闪烁效果 */
            tcol[i] = BRIGHT[rand() % BRIGHT_N];
        grid[tcy[i]][tcx[i]] = BLOCK;
        gridCol[tcy[i]][tcx[i]] = tcol[i];
    }
}

/* 只重画和上一帧不同的格子，避免整屏刷新闪烁 */
void render(void)
{
    int x, y;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            if (grid[y][x] != prev[y][x] || gridCol[y][x] != prevCol[y][x]) {
                gotoxy(x, y);
                setColor(gridCol[y][x]);
                printf("%s", CH[grid[y][x]]);
                prev[y][x] = grid[y][x];
                prevCol[y][x] = gridCol[y][x];
            }
    setColor(7);
}

/* 统计已点亮的文字像素数 */
int litCount(void)
{
    int i, n = 0;
    for (i = 0; i < tcellCount; i++)
        if (t >= tdelay[i]) n++;
    return n;
}

/* 把两行文字的点阵像素记录到列表里 */
void initText(void)
{
    int k, line, idx, r, c;
    tcellCount = 0;
    for (k = 0; k < CHARS; k++) {
        line = k / LINE_N;              /* 第几行 */
        idx  = k % LINE_N;              /* 行内第几个字 */
        for (r = 0; r < CHH; r++)
            for (c = 0; c < CW; c++)
                if (GLYPH[k][r][c] == '#') {
                    tcx[tcellCount] = TEXT_X0 + idx * STRIDE + c;
                    tcy[tcellCount] = TEXT_Y0 + line * (CHH + LINE_GAP) + r;
                    /* 金黄色为主，混一些红和白 */
                    tcol[tcellCount] = (rand() % 10 < 6) ? 14
                                     : BRIGHT[rand() % BRIGHT_N];
                    tcellCount++;
                }
    }
}

/* ---------- 主程序 ---------- */

int main(void)
{
    int i;
    HANDLE hOut;
    COORD bufSize = { W + 1, H + 3 };
    SMALL_RECT winRect = { 0, 0, W, H + 2 };

    SetConsoleOutputCP(65001);              /* 支持中文和 █ 显示 */

    /* 环境检查：必须是真正的终端（和 snake.c 一样） */
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_CHAR ||
        GetFileType(GetStdHandle(STD_INPUT_HANDLE))  != FILE_TYPE_CHAR) {
        printf("当前环境不是交互式终端，动画无法运行！\n");
        printf("请这样运行：\n");
        printf("  1. 打开终端（cmd 或 PowerShell）\n");
        printf("  2. 切换到 h:\\CYUYAN 目录\n");
        printf("  3. 输入 teacherday.exe 并回车\n");
        fflush(stdout);
        system("pause");
        return 1;
    }

    srand((unsigned)time(NULL));
    hideCursor();

    /* 尽量把窗口调成能装下画面的尺寸（失败也不影响运行） */
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleScreenBufferSize(hOut, bufSize);
    SetConsoleWindowInfo(hOut, TRUE, &winRect);

    /* 初始化画面缓冲 */
    memset(grid, 0, sizeof(grid));
    memset(gridCol, 0, sizeof(gridCol));
    memset(prev, 0xFF, sizeof(prev));       /* 故意和画面不同，第一帧全部画一遍 */
    memset(prevCol, 0xFF, sizeof(prevCol));
    initText();

    /* 动画主循环 */
    while (1) {
        t++;
        if (_kbhit()) { _getch(); break; }  /* 按任意键结束 */

        if (t < PHASE2) {                   /* 阶段1：纯烟花秀 */
            if (t == 1 || t % 8 == 0) launchRocket();
        } else {                            /* 阶段2：烟花 + 文字点亮 */
            if (t == PHASE2)
                for (i = 0; i < tcellCount; i++)
                    tdelay[i] = PHASE2 + rand() % 90;   /* 每格随机延迟点亮 */
            if (t % 18 == 0) launchRocket();
        }

        memset(grid, 0, sizeof(grid));      /* 清空这一帧 */
        memset(gridCol, 0, sizeof(gridCol));
        drawText();                         /* 画文字（阶段2才有） */
        drawParticles();
        drawRockets();
        updateRockets();                    /* 先画后动，下一帧位置生效 */
        updateParticles();
        render();                           /* 把变化的部分刷到屏幕上 */

        /* 底部状态栏 */
        gotoxy(0, H);
        setColor(7);
        if (t < PHASE2)
            printf("烟花绽放中... 已发射 %d 发   按任意键结束", (t + 7) / 8);
        else
            printf("教师节快乐！文字点亮 %d%%   按任意键结束", litCount() * 100 / tcellCount);
        printf("                                                            ");

        Sleep(40);                          /* 每帧 40 毫秒 ≈ 25 帧/秒 */
        if (t > 2400) break;                /* 96 秒兜底退出 */
    }

    /* 结束画面 */
    gotoxy(0, H + 1);
    setColor(14);
    printf("郑乖乖节日快乐");
    setColor(7);
    printf(" 按任意键退出...");
    showCursor();
    _getch();
    return 0;
}
