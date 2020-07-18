
#include "file.h"

file::file(std::string path)
    : m_path(path)
{
    
}

file::~file()
{
    if (m_stream.is_open()) m_stream.close();
}

void file::load()
{
    //m_stream.exceptions(std::ios::failbit);

    m_stream.open(m_path);
}

char file::read()
{
    return m_stream.get();
}

std::function<char()> file::get_reader()
{
    if (!m_loaded)
    {
        load();
    }
    return std::bind(&file::read, this);
}