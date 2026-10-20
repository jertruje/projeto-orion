#include <orion/package/package.h>
#include <assert.h>

void test_package_manager_init(void)
{
    assert(orion_package_manager.initialize());
}

int main(void)
{
    test_package_manager_init();
    return 0;
}
