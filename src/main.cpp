
#include "preprocessor.h"

#include <iostream>
#include <fstream>
#include <exception>

#define A "========================================"

int main(int argc, char** argv)
{
    file f("src/test.txt");
    preprocessor pp(f);

    std::cout << A << std::endl;
    while (true)
    {
        char c = pp.next_processed();

        if (!c) break;

        std::cout << c;
    }
    std::cout << std::endl << A << std::endl;

    return 0;
}
