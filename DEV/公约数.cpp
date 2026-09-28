#include <stdio.h>

int main()
{
    int a, b;
    printf("请输入两个正整数：");
    scanf("%d %d", &a, &b);

    while (b != 0)
    {
        int temp = a % b;  // 求余数
        a = b;             // 除数变被除数
        b = temp;          // 余数变除数
    }

    printf("最大公约数是：%d\n", a);

    return 0;
}
