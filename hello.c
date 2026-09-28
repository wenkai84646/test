#include <stdio.h>
#include <windows.h>   /* SetConsoleOutputCP */

//int main(void)
//{
//    printf("23+43=%d\n",23+43);
//
//    return 0
//}


int main()
{
//    SetConsoleOutputCP(65001);   /* 控制台改用 UTF-8 编码，中文才不乱码 */

    int price = 0;

    printf("请输入金额（元）:");
//    fflush(stdout);      /* 立刻把提示显示出来（printf 不带 \n 时会被缓冲） */

    scanf("%d",& price);

    int chang = 100 - price;

    printf("找您%d元\n",chang);

    return 0;

}