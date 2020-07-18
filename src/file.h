
#ifndef FILE_H
#define FILE_H

#include <string>
#include <fstream>
#include <functional>

class file
{
public:
    file(std::string path);
    ~file();

    std::function<char()> get_reader();

protected:
    std::ifstream m_stream;
    
    bool m_loaded = false;
    void load();

    char read();

    const std::string m_path;
};

#endif // FILE_H