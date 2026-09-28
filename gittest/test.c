#include <stdio.h>
#include <limits.h>  // 包含整数范围的定义

int main()
{
    printf("int的最小值：%d\n", INT_MIN);
    printf("int的最大值：%d\n", INT_MAX);

    int a = INT_MAX;
    printf("a = %d\n", a);
    a++;  // 溢出！
    printf("1a++ = %d\n", a);  // 变成了最小值（绕回去了）

    return 0;
}
