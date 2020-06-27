
#include "preprocessor.h"

#include <iostream>

preprocessor::preprocessor(preprocessor::reader_t reader)
    : m_reader(reader)
{  }

char preprocessor::next()
{
    if (m_out_queue.empty())
    {
        char c = next_char();

        if (m_block_status != UNBLOCKED)
        {
            return handle_block(c);
        }
        else if (c == '"')
        {
            m_block_status = STRING;
            return c;
        }
        else if (c == '/')
        {
            char c_n = next_char();

            if (c_n == '/' || c_n == '*')
            {
                m_block_status = c_n == '/' ? SL_COMMENT : ML_COMMENT;
                m_out_queue.push(c_n);
            }
            else
            {
                m_in_queue.push(c_n);
            }

            return c;
        }

        return process(c);
    }
    else
    {
        char front = m_out_queue.front();
        m_out_queue.pop();

        return front;
    }
}

char preprocessor::process(char c)
{
    //std::cout << c;

    return c;
}

char preprocessor::handle_block(char c)
{
    char c_n;

    if (m_block_status == STRING)
    {
        if (c != '"') return c;

        c_n = next_char();

        if (c_n != '"')
        {
            m_in_queue.push(c_n);
            m_block_status = UNBLOCKED;
        }
        else
        {
            m_out_queue.push(c_n);
        }
    }
    else if (m_block_status == SL_COMMENT)
    {
        if (c == '\n')
        {
            m_block_status = UNBLOCKED;
        }
    }
    else if (m_block_status == ML_COMMENT)
    {
        if (c != '*') return c;

        c_n = next_char();

        if (c_n != '/')
        {
            m_in_queue.push(c_n);
        }
        else
        {
            m_out_queue.push(c_n);
            m_block_status = UNBLOCKED;
        }
    }

    return c;
}

char preprocessor::next_char()
{
    char c;

    if (!m_in_queue.empty())
    {
        c = m_in_queue.front();
        m_in_queue.pop();
    }
    else
    {
        c = m_reader();
    }

    return c;
}