
#include "preprocessor.h"

#include <exception>
#include <ctype.h>
#include <algorithm>
#include <unordered_map>

static const std::unordered_map<std::string, std::regex> patterns = {
    {"identifier", "^[a-zA-Z_][\\da-zA-Z_]*$"},
    {"alha", "[a-zA-Z]*"}
};

preprocessor::preprocessor(preprocessor::reader_t reader)
    : m_readers({reader})
{  }

char preprocessor::next()
{
    if (m_out_queue.empty())
    {
        char c = next_char();

        if (m_block_status == BLOCKED_BY_USER)
        {
            return c;
        }
        else if (m_block_status != UNBLOCKED)
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
    if (c == '#')
    {
        std::string instruction = get_regex(patterns["alpha"]);
        std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        if (instruction == "define")
        {

        }
        else if (instruction == "include")
        {

        }
        else if (instruction == "ifdef" || instruction == "ifndef")
        {
            if (m_control_state != CLEAR)
            {
                throw std::invalid_argument("Unexpected " + instruction + ", already inside control statement");
            }

            std::string macro = get_regex(patterns["identifier"]);
            std::transform(macro.begin(), macro.end(), macro.begin(), ::tolower);

            bool is_defined = m_macros.find(macro) != m_macros.end();
            bool truthy = instruction == "ifdef" ? is_defined : !is_defined;

            m_control_state = IFSTMT | (truthy ? 0 : BLOCKED);
        }
        else if (instruction == "else")
        {
            if (m_control_state)
            {
                m_control_state &= ~(m_control_state & BLOCKED);
            }
            else
            {
                throw std::invalid_argument("Unexpected else");
            }
            
        }
        else if (instruction == "endif")
        {
            if (m_control_state)
            {
                m_control_state = CLEAR;
            }
            else
            {
                throw std::invalid_argument("Unexpected endif");
            }
        }
        else if (instruction == "undef")
        {
            std::string macro = get_regex(patterns["identifier"]);

            m_macros.erase(macro);
        }
    }
    else if (c == '_' || isalpha(c))
    {
        std::string identifier = get_regex(patterns["identifier"]);
        std::transform(identifier.begin(), identifier.end(), identifier.begin(), ::tolower);

        auto mac = m_macros.find(identifier);

        if (mac != m_macros.end())
        {
            // add_reader(*mac);
        }
    }

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
        reader_t reader = this->get_reader();
        try
        {
            c = reader();
        }
        catch (const std::out_of_range& e)
        {
            m_readers.pop();

            return next_char();
        }
    }

    return c;
}

void preprocessor::add_reader(reader_t& reader)
{
    m_readers.push(reader);
}

preprocessor::reader_t preprocessor::get_reader()
{
    if (m_readers.empty())
    {
        throw std::out_of_range("No more readers left");
    }

    return m_readers.top();
}

std::string preprocessor::get_regex(std::regex re)
{
    std::string s = "", tmp_s = s;
    char c;
    
    while (true)
    {
        c = next_char();
        tmp_s += c;

        if (std::regex_match(tmp_s, re))
        {
            s = tmp_s;
        }
        else
        {
            m_in_queue.push(c);
            break;
        }
    }

    return s;
}
