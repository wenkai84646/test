#include <stdio.h>

int main()
{
   float score;
   printf("请输入你对我的好感度");
   scanf("%f",&score);
   if(score>=80)
{
	printf("love");
}
   	else
{
	   	printf("请提升对我的好感度");
}

    return 0;
}
