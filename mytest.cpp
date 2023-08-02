#include <iostream>
#include <vector>
using namespace  std;

void* p = ((void*)-1);
void *p2 = nullptr;

int main()
{
    cout<<(p==p2)<<endl;

    return 0;
}