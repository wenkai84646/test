/* ============================================================
 * 终端烟花 yanhua.c
 *
 * 效果：
 *   1. 烟花一枚接一枚升空，在天空炸开成彩色火花
 *   2. 火花在重力下散落，慢慢熄灭
 *   3. 最后所有火花重新聚集，拼出 "zhy520" 并闪烁
 *
 * 原理：每一帧先把整幅画面画进内存缓冲区，
 *       再一次调用 WriteConsoleOutputW 刷到控制台，所以不闪烁。
 *
 * 编译：gcc yanhua.c -o yanhua    （MinGW 不用加 -lm）
 * 运行：.\yanhua
 * 提示：把终端拉宽一点（110 列以上）效果最好；Ctrl+C 可随时退出
 * ============================================================ */

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <windows.h>

#define MAXW      160      /* 画面最大宽度 */
#define H         30       /* 画面高度（行） */
#define MAXSPARK  1500     /* 火花数量上限 */
#define NROCKET   8        /* 一共放几发烟花 */

int W = 120;               /* 画面实际宽度，启动时按终端宽度调整 */

/* ---------- 一颗火花：爆炸后飞散的小亮点 ---------- */
typedef struct {
    float x, y;            /* 当前位置 */
    float vx, vy;          /* 当前速度 */
    float tx, ty;          /* 目标点（最后组字阶段用） */
    int   color;           /* 颜色编号，对应下面的 palette */
    int   life, maxlife;   /* 剩余寿命 / 总寿命（单位：帧） */
    int   alive;
} Spark;

Spark sparks[MAXSPARK];
int nspark = 0;

/* ---------- 一枚火箭：升空阶段的亮点 ---------- */
typedef struct {
    float x, y, vy;        /* 位置和垂直速度 */
    int   color;
    int   alive;
} Rocket;

Rocket rockets[NROCKET];
int nrocket = 0;

/* ---------- 颜色表：Windows 控制台的 7 种颜色 ---------- */
const WORD palette[7] = {
    FOREGROUND_RED,                                         /* 红 */
    FOREGROUND_RED | FOREGROUND_GREEN,                      /* 黄 */
    FOREGROUND_GREEN,                                       /* 绿 */
    FOREGROUND_GREEN | FOREGROUND_BLUE,                     /* 青 */
    FOREGROUND_BLUE,                                        /* 蓝 */
    FOREGROUND_RED | FOREGROUND_BLUE,                       /* 紫 */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,    /* 白 */
};

/* ---------- 画面缓冲区：每个格子 = 一个字符 + 一个颜色 ---------- */
CHAR_INFO frame[H][MAXW];
HANDLE hOut;

/* 演出阶段：升空 -> 组字 -> 闪烁 -> 结束 */
enum { PHASE_LAUNCH, PHASE_TEXT, PHASE_HOLD, PHASE_DONE };
int g_phase = PHASE_LAUNCH;
int g_frame = 0;

/* ---------- 控制台相关 ---------- */

/* 把光标显示出来（正常结束和 Ctrl+C 退出时都要恢复） */
static void cursor_show(void)
{
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &ci);
}

/* Ctrl+C 退出时先恢复光标再结束 */
static BOOL WINAPI ctrl_handler(DWORD type)
{
    cursor_show();
    ExitProcess(0);
    return TRUE;
}

static void console_init(void)
{
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    /* 按终端实际宽度来画（最宽 160 列，最窄 40 列） */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        W = csbi.dwSize.X;
        if (W > MAXW) W = MAXW;
        if (W < 40)   W = 40;
    } else {
        W = 80;      /* 拿不到终端信息就按 80 列画 */
    }

    /* 藏起光标，不然一闪一闪破坏画面 */
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);

    SetConsoleCtrlHandler(ctrl_handler, TRUE);
}

/* 往缓冲区画一个字符（超出屏幕的自动忽略） */
static void plot(int x, int y, wchar_t ch, WORD attr)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    frame[y][x].Char.UnicodeChar = ch;
    frame[y][x].Attributes = attr;
}

/* ---------- 烟花逻辑 ---------- */

/* 爆炸：在 (cx, cy) 处一次炸出 50~89 颗火花，朝四面八方飞 */
static void explode(float cx, float cy, int color)
{
    int i, n = 50 + rand() % 40;
    for (i = 0; i < n && nspark < MAXSPARK; i++) {
        Spark *s = &sparks[nspark++];
        float ang = (float)(rand() % 360) * 3.14159f / 180.0f;
        float spd = 0.25f + (float)(rand() % 95) / 100.0f;   /* 0.25 ~ 1.2 */
        s->x = cx;
        s->y = cy;
        s->vx = cosf(ang) * spd;
        s->vy = sinf(ang) * spd;
        s->tx = 0;
        s->ty = 0;
        s->color = color;
        s->maxlife = 50 + rand() % 40;                       /* 活 50~89 帧 */
        s->life = s->maxlife;
        s->alive = 1;
    }
}

/* 更新爆炸火花：重力往下拉 + 空气阻力减速 + 寿命递减 */
static void update_explosion_sparks(void)
{
    int i;
    for (i = 0; i < nspark; i++) {
        Spark *s = &sparks[i];
        if (!s->alive) continue;
        s->vy += 0.012f;      /* 重力 */
        s->vx *= 0.97f;       /* 空气阻力 */
        s->vy *= 0.97f;
        s->x += s->vx;
        s->y += s->vy;
        s->life--;
        if (s->life <= 0) s->alive = 0;
    }
}

/* 更新火箭：一路升空、越飞越慢，快停住时爆炸 */
static void update_rockets(void)
{
    int i;
    for (i = 0; i < nrocket; i++) {
        Rocket *r = &rockets[i];
        if (!r->alive) continue;
        r->y += r->vy;
        r->vy += 0.03f;                 /* 向上的速度慢慢衰减 */
        if (r->vy >= -0.35f) {          /* 快到最高点了，炸！ */
            explode(r->x, r->y, r->color);
            r->alive = 0;
        }
    }
}

/* ---------- 渲染：先画进缓冲区，最后一次性刷屏 ---------- */

static void render(void)
{
    int x, y, i;

    /* 1. 清空成黑底 */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            frame[y][x].Char.UnicodeChar = L' ';
            frame[y][x].Attributes = 0;
        }

    /* 2. 画火箭（亮头 + 两段拖尾） */
    for (i = 0; i < nrocket; i++) {
        Rocket *r = &rockets[i];
        if (!r->alive) continue;
        plot((int)r->x, (int)r->y, L'#',
             palette[r->color] | FOREGROUND_INTENSITY);
        plot((int)r->x, (int)(r->y + 2), L'*', palette[r->color]);
        plot((int)r->x, (int)(r->y + 4), L'.', palette[r->color]);
    }

    /* 3. 画火花：越接近熄灭越暗，中间阶段带拖尾 */
    if (g_phase == PHASE_LAUNCH) {
        for (i = 0; i < nspark; i++) {
            Spark *s = &sparks[i];
            float f;                       /* 剩余寿命比例：1=刚炸开 */
            WORD base;
            if (!s->alive) continue;
            f = (float)s->life / (float)s->maxlife;
            base = palette[s->color];
            if (f > 0.65) {
                plot((int)s->x, (int)s->y, L'#', base | FOREGROUND_INTENSITY);
            } else if (f > 0.35) {
                plot((int)s->x, (int)s->y, L'o', base | FOREGROUND_INTENSITY);
                plot((int)(s->x - s->vx), (int)(s->y - s->vy), L'.', base);
            } else {
                plot((int)s->x, (int)s->y, L'.', base);
            }
        }
    } else {
        /* 组字 / 闪烁阶段：火花飞向自己的目标点，带拖尾 */
        for (i = 0; i < nspark; i++) {
            Spark *s = &sparks[i];
            wchar_t ch;
            if (!s->alive) continue;
            if (g_phase == PHASE_HOLD && (i + g_frame / 5) % 7 == 0)
                ch = L'o';                 /* 拼好之后一闪一闪 */
            else
                ch = L'#';
            plot((int)s->x, (int)s->y, ch,
                 palette[s->color] | FOREGROUND_INTENSITY);
            plot((int)(s->x - s->vx), (int)(s->y - s->vy), L'.',
                 palette[s->color]);
        }
    }

    /* 4. 一次性把整幅画面写到控制台（这样才不闪烁） */
    {
        COORD size = { (SHORT)W, (SHORT)H };
        COORD pos = { 0, 0 };
        SMALL_RECT rect = { 0, 0, (SHORT)(W - 1), (SHORT)(H - 1) };
        WriteConsoleOutputW(hOut, (CHAR_INFO *)frame, size, pos, &rect);
    }
}

/* ---------- 组字阶段：把 "zhy520" 变成一颗颗火花的终点 ---------- */

/* 5x7 点阵字库：z h y 5 2 0
 * 每个字符 5 列，每列一个字节，bit0 是字的最上面一行 */
static const unsigned char FONT[6][5] = {
    { 0x21, 0x31, 0x29, 0x25, 0x23 },   /* z */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F },   /* h */
    { 0x07, 0x0F, 0x08, 0x08, 0x7F },   /* y */
    { 0x4F, 0x49, 0x49, 0x49, 0x79 },   /* 5 */
    { 0x79, 0x49, 0x49, 0x49, 0x4F },   /* 2 */
    { 0x3E, 0x61, 0x61, 0x61, 0x3E },   /* 0 */
};

/* 把每个亮点的屏幕坐标算出来，存进 sparks[].tx / ty */
static void setup_text(void)
{
    int sw, sh;                       /* 每个点阵像素放大成 sw x sh 格 */
    int c, col, row, dx, dy;
    int tw, th, x0, y0, n = 0;

    if (W >= 110)     { sw = 3; sh = 2; }   /* 窗口宽：字放大一点 */
    else if (W >= 74) { sw = 2; sh = 1; }
    else              { sw = 1; sh = 1; }

    tw = 6 * 6 * sw;                    /* 6 个字符，每个宽 (5 列 + 1 间距) */
    th = 7 * sh;
    x0 = (W - tw) / 2;                  /* 居中 */
    y0 = (H - th) / 2;

    for (c = 0; c < 6; c++) {                          /* 第几个字符 */
        for (col = 0; col < 5; col++) {                /* 第几列 */
            for (row = 0; row < 7; row++) {            /* 第几行 */
                if ((FONT[c][col] & (1 << row)) == 0) continue;  /* 灭点跳过 */
                for (dy = 0; dy < sh; dy++) {
                    for (dx = 0; dx < sw; dx++) {
                        sparks[n].tx = (float)(x0 + (c * 6 + col) * sw + dx);
                        sparks[n].ty = (float)(y0 + row * sh + dy);
                        n++;
                    }
                }
            }
        }
    }
    nspark = n;
}

/* 所有火花从屏幕中央炸开，然后被"弹簧"拉向自己的目标点 */
static void spawn_text_sparks(void)
{
    int i;
    for (i = 0; i < nspark; i++) {
        Spark *s = &sparks[i];
        float ang = (float)(rand() % 360) * 3.14159f / 180.0f;
        float spd = 0.8f + (float)(rand() % 170) / 100.0f;
        s->x = W / 2.0f;
        s->y = H / 2.0f;
        s->vx = cosf(ang) * spd;
        s->vy = sinf(ang) * spd;
        s->color = rand() % 7;
        s->maxlife = 1000;              /* 组字火花不会自己熄灭 */
        s->life = 1000;
        s->alive = 1;
    }
}

/* 弹簧物理：加速度 = (目标 - 位置) * k - 速度 * 阻尼
 * 火花会先冲过头再弹回来，来回振荡几次后稳稳停在目标点上 */
static void update_text_sparks(void)
{
    int i;
    float maxdist = 0.0f;
    for (i = 0; i < nspark; i++) {
        Spark *s = &sparks[i];
        float d;
        s->vx += (s->tx - s->x) * 0.02f - s->vx * 0.08f;
        s->vy += (s->ty - s->y) * 0.02f - s->vy * 0.08f;
        s->x += s->vx;
        s->y += s->vy;
        d = fabsf(s->tx - s->x) + fabsf(s->ty - s->y);
        if (d > maxdist) maxdist = d;
    }
    if (maxdist < 1.2f)
        g_phase = PHASE_HOLD;      /* 所有火花都到位了，开始闪烁 */
}

/* ---------- 主程序 ---------- */

int main(void)
{
    int i;
    int pframe = 0;               /* 当前阶段已经跑了多少帧 */

    srand((unsigned)time(NULL));
    console_init();

    /* 第一发火箭立刻升空 */
    rockets[0].x = 10 + rand() % (W - 20);
    rockets[0].y = H - 1;
    rockets[0].vy = -(0.75f + (float)(rand() % 20) / 100.0f);
    rockets[0].color = rand() % 7;
    rockets[0].alive = 1;
    nrocket = 1;

    while (g_phase != PHASE_DONE) {
        g_frame++;

        if (g_phase == PHASE_LAUNCH) {
            pframe++;
            /* 每 30 帧再放一发，一共 8 发 */
            if (nrocket < NROCKET && pframe % 30 == 0) {
                Rocket *r = &rockets[nrocket++];
                r->x = 10 + rand() % (W - 20);
                r->y = H - 1;
                r->vy = -(0.75f + (float)(rand() % 20) / 100.0f);
                r->color = rand() % 7;
                r->alive = 1;
            }
            update_rockets();
            update_explosion_sparks();

            /* 8 发全放完、所有火花都熄灭了 → 进入组字阶段 */
            {
                int any = 0;
                if (nrocket >= NROCKET) {
                    for (i = 0; i < nrocket; i++) any |= rockets[i].alive;
                    for (i = 0; i < nspark; i++)   any |= sparks[i].alive;
                } else {
                    any = 1;
                }
                if (!any) {
                    setup_text();
                    spawn_text_sparks();
                    g_phase = PHASE_TEXT;
                    pframe = 0;
                }
            }
        } else if (g_phase == PHASE_TEXT) {
            pframe++;
            update_text_sparks();
            if (g_phase == PHASE_TEXT && pframe > 500)
                g_phase = PHASE_HOLD;   /* 保险：最多 500 帧强制进入闪烁 */
        } else if (g_phase == PHASE_HOLD) {
            pframe++;
            if (pframe > 220) g_phase = PHASE_DONE;   /* 闪烁约 6 秒后结束 */
        }

        render();
        Sleep(30);                    /* 约 33 帧/秒 */
    }

    cursor_show();                    /* 演出结束，把光标还回来 */
    return 0;
}
