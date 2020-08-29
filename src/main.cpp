
#include "preprocessor.h"

#include <iostream>
#include <fstream>
#include <exception>

#define A "========================================"

int main(int argc, char** argv)
{
    auto ptr = std::make_unique<std::ifstream>("src/test.txt");
    //preprocessor pp(static_cast<std::unique_ptr<std::istream>>(std::move(is)));

    preprocessor pp(std::move(ptr));
    int i = 0;

    std::cout << A << std::endl;
    while (true)
    {
        i++;
        char c = pp.next();

        if (c == 0 || i > 100) break;

        std::cout << c;
    }
    std::cout << std::endl << A << std::endl;

    return 0;
}
