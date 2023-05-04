cmake_minimum_required(VERSION 3.22.1)

add_executable(test main.c test.c CuTest.c fileSystem.c device.c)
add_executable(display display.c device.c)
add_executable(before beforeTest.c fileSystem.c device.c)
add_executable(lol lol.c device.c)
add_executable(after afterTest.c fileSystem.c device.c)

