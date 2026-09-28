/* 贪吃蛇小游戏（Windows 控制台版）
 * 编译：gcc snake.c -o snake.exe
 * 操作：方向键或 WASD 移动，Q 退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>      /* _kbhit / _getch 无回显按键 */
#include <windows.h>    /* 控制台 API */
#include <time.h>

#define WIDTH   30              /* 游戏区域宽（列） */
#define HEIGHT  20              /* 游戏区域高（行） */
#define MAX_LEN (WIDTH * HEIGHT) /* 蛇身最大长度 */

int snakeX[MAX_LEN], snakeY[MAX_LEN]; /* 蛇身每节坐标，[0] 是蛇头 */
int length  = 3;    /* 当前蛇长 */
int foodX, foodY;   /* 食物位置 */
int score   = 0;    /* 得分 */
int dir     = 4;    /* 移动方向：1上 2下 3左 4右 */
int speed   = 150;  /* 每步间隔毫秒数，越小越快 */
int gameOver = 0;   /* 游戏是否结束 */
int win     = 0;    /* 是否胜利（蛇占满整个区域） */

/* 把光标移动到第 y 行第 x 列（从 0 开始） */
void gotoxy(int x, int y)
{
    COORD pos = { x, y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

/* 隐藏光标，避免画面闪烁 */
void hideCursor(void)
{
    CONSOLE_CURSOR_INFO ci;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(h, &ci);
}

/* 设置文字颜色 */
void setColor(int color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

/* 画四周墙壁 */
void drawBorder(void)
{
    int i;
    setColor(7); /* 白色 */
    gotoxy(0, 0);
    printf("+");
    for (i = 1; i <= WIDTH; i++) printf("-");
    printf("+");
    for (i = 1; i <= HEIGHT; i++) {
        gotoxy(0, i);         printf("|");
        gotoxy(WIDTH + 1, i); printf("|");
    }
    gotoxy(0, HEIGHT + 1);
    printf("+");
    for (i = 1; i <= WIDTH; i++) printf("-");
    printf("+");
}

/* 判断某格是否在蛇身上 */
int onSnake(int x, int y)
{
    int i;
    for (i = 0; i < length; i++)
        if (snakeX[i] == x && snakeY[i] == y)
            return 1;
    return 0;
}

/* 随机生成一个不在蛇身上的食物 */
void spawnFood(void)
{
    do {
        foodX = rand() % WIDTH;
        foodY = rand() % HEIGHT;
    } while (onSnake(foodX, foodY));
    setColor(12); /* 红色 */
    gotoxy(foodX + 1, foodY + 1);
    printf("$");
}

/* 读取按键并改变方向（禁止原地 180 度掉头） */
void handleInput(void)
{
    int ch, nd;
    if (!_kbhit()) return;      /* 没有按键就直接返回 */
    ch = _getch();
    nd = dir;

    if (ch == 224 || ch == 0) { /* 方向键是两个字节的扩展码 */
        ch = _getch();
        switch (ch) {
        case 72: nd = 1; break; /* 上 */
        case 80: nd = 2; break; /* 下 */
        case 75: nd = 3; break; /* 左 */
        case 77: nd = 4; break; /* 右 */
        }
    } else {
        switch (ch) {
        case 'w': case 'W': nd = 1; break;
        case 's': case 'S': nd = 2; break;
        case 'a': case 'A': nd = 3; break;
        case 'd': case 'D': nd = 4; break;
        case 'q': case 'Q': case 27: /* ESC */
            gameOver = 1;
            return;
        }
    }

    /* 新方向和当前方向相反则忽略（不能掉头撞自己） */
    if (!((dir == 1 && nd == 2) || (dir == 2 && nd == 1) ||
          (dir == 3 && nd == 4) || (dir == 4 && nd == 3)))
        dir = nd;
}

/* 前进一步：碰撞检测 + 吃食物判断 */
void update(void)
{
    int nx = snakeX[0], ny = snakeY[0];
    int tailX, tailY, i;

    /* 计算新蛇头位置 */
    switch (dir) {
    case 1: ny--; break;
    case 2: ny++; break;
    case 3: nx--; break;
    case 4: nx++; break;
    }

    /* 撞墙 */
    if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) {
        gameOver = 1;
        return;
    }
    /* 撞到自己（蛇头不能碰到身体） */
    for (i = 1; i < length; i++)
        if (snakeX[i] == nx && snakeY[i] == ny) {
            gameOver = 1;
            return;
        }

    tailX = snakeX[length - 1];
    tailY = snakeY[length - 1];

    /* 每节身体移到前一节的位置 */
    for (i = length - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }
    snakeX[0] = nx;
    snakeY[0] = ny;

    if (nx == foodX && ny == foodY) {   /* 吃到食物 */
        snakeX[length] = tailX;         /* 尾巴保留，蛇变长一节 */
        snakeY[length] = tailY;
        length++;
        score += 10;
        if (speed > 60) speed -= 10;    /* 越吃越快 */
        setColor(14);
        gotoxy(0, HEIGHT + 3);
        printf("得分: %d", score);

        if (length >= MAX_LEN) {        /* 蛇占满整个区域，胜利 */
            win = 1;
            gameOver = 1;
        } else {
            spawnFood();
        }
    } else {                            /* 没吃到：擦掉旧尾巴 */
        setColor(7);
        gotoxy(tailX + 1, tailY + 1);
        printf(" ");
    }

    /* 旧蛇头变成身体，画新蛇头 */
    setColor(10); /* 绿色身体 */
    gotoxy(snakeX[1] + 1, snakeY[1] + 1);
    printf("#");
    setColor(14); /* 黄色蛇头 */
    gotoxy(snakeX[0] + 1, snakeY[0] + 1);
    printf("@");
}

int main(void)
{
    int i;

    SetConsoleOutputCP(65001);          /* 让控制台支持中文显示 */

    /* 本游戏必须运行在真正的终端里。
       IDE 的输出面板（Code Runner 等）、重定向环境下，
       光标定位和按键读取都会失效，直接提示怎么运行并退出 */
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_CHAR ||
        GetFileType(GetStdHandle(STD_INPUT_HANDLE))  != FILE_TYPE_CHAR) {
        printf("当前环境不是交互式终端，游戏无法运行！\n");
        printf("请这样运行：\n");
        printf("  1. 打开终端（cmd 或 PowerShell）\n");
        printf("  2. 切换到 h:\\CYUYAN 目录\n");
        printf("  3. 输入 snake.exe 并回车\n");
        fflush(stdout);
        system("pause");
        return 1;
    }

    srand((unsigned)time(NULL));        /* 随机数种子 */
    hideCursor();
    drawBorder();

    /* 初始蛇：水平 3 节，蛇头在中间，向右移动 */
    for (i = 0; i < length; i++) {
        snakeX[i] = WIDTH / 2 - i;
        snakeY[i] = HEIGHT / 2;
    }
    setColor(10);
    for (i = 1; i < length; i++) {
        gotoxy(snakeX[i] + 1, snakeY[i] + 1);
        printf("#");
    }
    setColor(14);
    gotoxy(snakeX[0] + 1, snakeY[0] + 1);
    printf("@");
    setColor(7);
    gotoxy(0, HEIGHT + 3);
    printf("得分: 0    方向键/WASD 移动，Q 退出");
    spawnFood();

    /* 游戏主循环 */
    while (!gameOver) {
        handleInput();
        if (gameOver) break;
        update();
        Sleep(speed);
    }

    /* 游戏结束 */
    setColor(12);
    gotoxy(0, HEIGHT + 5);
    if (win)
        printf("恭喜你赢了！最终得分: %d\n", score);
    else
        printf("游戏结束！最终得分: %d\n", score);
    setColor(7);
    printf("按任意键退出...");
    _getch();
    return 0;
}
