
#include "preprocessor.h"

#include <fstream>
#include <exception>

int main(int argc, char** argv)
{
    std::ifstream fs("src/test.txt");

#if 1
    auto p = preprocessor([&fs](){
        char c;

        if (fs.get(c))
            return c;
        else
            throw std::out_of_range((char*)&c);
    });

    while (true)
    {
        try {
            std::cout << p.next();
        }
        catch (std::out_of_range e)
        {
            break;
        }
    };
#else
    char c;
    while (fs.get(c))
        std::cout << c;
#endif

    return 0;
}
