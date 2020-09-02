
#include "preprocessor.h"

#include <iostream>
#include <fstream>
#include <exception>

#define IN_PATH "sbx/in/file1.txt"
#define OUT_PATH "sbx/out/file1.txt"

int main(int argc, char** argv)
{
    auto ptr = std::make_shared<std::ifstream>(IN_PATH);
    auto outstr = std::ofstream(OUT_PATH);
    //preprocessor pp(static_cast<std::unique_ptr<std::istream>>(std::move(is)));

    preprocessor pp(std::move(ptr));

    for (char c; c = pp.next();)
    {
        outstr << c;
    }


    return 0;
}
