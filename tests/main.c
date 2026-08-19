#include <stdio.h>

extern void test_json_parser(void);
extern void test_arena(void);

int main(void)
{
    test_json_parser();
    test_arena();
    return 0;
}
