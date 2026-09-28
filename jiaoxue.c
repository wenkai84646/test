#include <stdio.h>                  // printf 所在的标准输入输出库
#include <windows.h>                // Windows 控制台函数

// 成功的交换函数：参数是"指针"（地址），顺着地址直接改外面的原变量
void swap(int *a, int *b)           // int *a 表示：a 是存"地址"的变量，指向一个 int
{
    int temp = *a;                  // *a 表示"a 指向的那个变量" → 把它的值暂存到 temp
    *a = *b;                        // 把 *b（b 指向的变量）的值放进 *a
    *b = temp;                      // 把暂存的值放进 *b
}

int main()                          // 主函数
{
    SetConsoleOutputCP(65001);      // 终端切成 UTF-8，中文不乱码

    int x = 5, y = 10;              // 定义两个变量，x=5、y=10
    printf("交换前：x=%d, y=%d\n", x, y);

    swap(&x, &y);                   // &x 表示"x 的地址"，把地址传进函数

    printf("交换后：x=%d, y=%d\n", x, y);   // 这次真的交换成功了！

    return 0;
}

/*
 * ========== 失败的版本（留着对照，随时可删） ==========
 * 为什么失败？C 函数的参数是"传值"：
 * 调用 swap(x, y) 时，函数里的 a、b 只是 x、y 的"复印件"，
 * 改复印件不影响原件，所以函数结束 x、y 纹丝不动。
 * 传地址就不同了：函数顺着地址找到原件，直接改原件。
 *
 * void swap(int a, int b)
 * {
 *     int temp = a;
 *     a = b;
 *     b = temp;
 *     printf("函数内：a=%d, b=%d\n", a, b);
 * }
 */
