#include <stdlib.h>
int bar(int c, int d);
void notmain(int a, int b)
{
  if (a > b)
  {
    if(a+1 > b)
    {
      b = b + abs(a);
      b = bar(a+1,b+2);
    }
    else
    {
      b = b + 1;
    }
  }
  else
  {
    int x = 3;
    while(x > 0)
    {
      a++;
      x--;
    }
    b = b + 2;
  }
}
int bar(int c, int d)
{
  if(c > d)
    return c+d;
  else
    return c-d;
}