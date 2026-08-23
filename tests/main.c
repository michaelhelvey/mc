#include <stdio.h>

extern void test_json_parser(void);
extern void test_arena(void);
extern void test_http_client(void);
extern void test_common(void);

int main(void)
{
    test_json_parser();
    test_arena();
    test_http_client();
    test_common();
    return 0;
}
