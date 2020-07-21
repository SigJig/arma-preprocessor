
#include "preprocessor.h"

#include <iostream>
#include <fstream>
#include <exception>

#define A "========================================"

int main(int argc, char** argv)
{
    std::unique_ptr<std::istream> is(new std::ifstream("src/test.txt"));
    //preprocessor pp(static_cast<std::unique_ptr<std::istream>>(std::move(is)));

    preprocessor pp(std::move(is));

    std::cout << A << std::endl;
    while (true)
    {
        char c = pp.next();

        if (!c) break;

        std::cout << c;
    }
    std::cout << std::endl << A << std::endl;

    return 0;
}
