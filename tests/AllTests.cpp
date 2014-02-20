#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"

int main(int argc, char **argv) {
    TestRegistry* r = TestRegistry::getCurrentRegistry();
    SetPointerPlugin ps("PointerStore");
    r->installPlugin(&ps);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

