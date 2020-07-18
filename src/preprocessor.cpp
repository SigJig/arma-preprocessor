
#include "preprocessor.h"

#include <exception>
#include <ctype.h>
#include <algorithm>
#include <unordered_map>

static std::unordered_map<std::string, std::function<bool(char)>> callbacks {
    {"identifier", [](char c) -> bool {
        static bool is_first = true;

        if (is_first)
        {
            is_first = false;
            return c == '_' || isalpha(c);
        }

        return c == '_' || isalnum(c);
    }}
};


macro::macro(std::string name, std::vector<std::string> args, std::string content)
    : m_name(name), m_args(args), m_content(content)
{
    
}

macro::instance::instance(std::vector<std::string> args, macro* parent)
    : m_args(args), m_parent(parent)
{

}

macro::instance macro::make_instance(std::vector<std::string> args)
{
    if (args.size() != m_args.size())
    {
        // throw exception
    }

    return instance(args, this);
}

preprocessor::preprocessor(preprocessor::reader_t reader)
    : m_readers({reader})
{  }

char preprocessor::next_raw()
{
    char c;

    if (!m_in_queue.empty())
    {
        c = m_in_queue.front();
        m_in_queue.pop();
    }
    else
    {
        while (true)
        {
            reader_t reader = get_reader();
            c = reader();

            if (c == 0) m_readers.pop();

            return c;
        }
    }

    return c;
}

char preprocessor::next_unprocessed()
{
    while (true) {
        char c = next_raw();
        char c_n;

        if (m_block_status == SL_COMMENT)
        {
            if (c == '\n')
            {
                m_block_status = UNBLOCKED;
            }
        }
        else if (m_block_status == ML_COMMENT)
        {
            if (c == '*')
            {
                c_n = next_raw();

                if (c_n != '/')
                {
                    put_back(c_n);
                }
                else
                {
                    // fast_track(c_n);
                    m_block_status = UNBLOCKED;
                }
            }
        }
        else if (c == '/')
        {
            c_n = next_raw();

            if (c_n == '/' || c_n == '*')
            {
                m_block_status = c_n == '/' ? SL_COMMENT : ML_COMMENT;
                //fast_track(c_n);
            }
            else
            {
                put_back(c_n);
                return c;
            }
        }
        else
        {
            return c;
        }
    }
}

// public method
char preprocessor::next_processed()
{
    if (m_out_queue.empty())
    {
        char c = next_unprocessed();

        if (m_block_status == BLOCKED_BY_USER)
        {
            return c;
        }
        else if (c == '"')
        {
            m_block_status = (m_block_status == STRING) ? UNBLOCKED : STRING;
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
        std::string instruction = make_string(callbacks["identifier"]);
        std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        if (instruction == "define")
        {
            std::string mac_name = make_string(callbacks["identifier"]);
            std::vector<std::string> args;

            char c_n = next_unprocessed();

            if (c_n == '(')
            {
                c_n = next_unprocessed();

                while (c_n != ')')
                {
                    std::string arg;
                    for (; c_n != ',' && c_n != ')'; c_n = next_unprocessed())
                    {
                        if (!(isalpha(c_n) || c_n == '_'))
                        {
                            // throw exception
                        }
                        else
                        {
                            arg += c_n;
                        }
                    }
                    args.push_back(arg);
                    arg = "";
                }
            }
            else
            {
                put_back(c_n);
            }
            
            std::string content = make_string([](char c) -> bool {
                static bool is_escaped = false;

                if (c == '\n')
                {
                    // if the character is a newline and it is not preceded by a '\'
                    if (!is_escaped) return false;
                    else { is_escaped = false; }
                }
                else if (c == '\\') is_escaped = !is_escaped;
                else if (!isspace(c)) is_escaped = false;

                return true;
            });

            m_macros.emplace(mac_name, macro(mac_name, args, content));
        }
        else if (instruction == "include")
        {
            std::string s;

            if (s[0] == '"' && s.back() == '"')
            {
                s.erase(s.begin());
                s.erase(s.end());
            }

            // add reader
        }
        else if (instruction == "ifdef" || instruction == "ifndef")
        {
            if (m_control_state != CLEAR)
            {
                throw std::invalid_argument("Unexpected " + instruction + ", already inside control statement");
            }

            std::string macro = make_string(callbacks["identifier"]);
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
            std::string macro = make_string(callbacks["identifier"]);
            std::transform(macro.begin(), macro.end(), macro.begin(), ::tolower);

            m_macros.erase(macro);
        }
    }
    else if (c == '_' || isalpha(c))
    {
        std::string identifier = make_string(callbacks["identifier"]);

        // Keep a seperate variable for the lowered identifer, so that if
        // it is not a macro we can put back the original characters.
        std::string lowered = identifier;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

        const auto mac = m_macros.find(lowered);

        if (mac != m_macros.end())
        {
            std::vector<std::string > args;

            char c_n = next_unprocessed();

            if (c_n == '(')
            {
                while (next_unprocessed() != ')')
                {
                    args.push_back(make_string([](char c) -> bool {
                        return c != ',' && c != ')';
                    }));
                }
            }
            else
            {
                put_back(c_n);
            }

            add_reader(mac->second.make_instance(args));
        }
        else
        {
            for (auto c : identifier) fast_track(c);
        }
    }

    return c;
}

void preprocessor::add_reader(const reader_t& reader)
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

void preprocessor::put_back(char c)
{
    m_in_queue.push(c);
}

void preprocessor::fast_track(char c)
{
    m_out_queue.push(c);
}

std::string preprocessor::make_string(std::function<bool(char)> callback)
{
    std::string s;
    
    char c = next_unprocessed();
    for (; callback(c); c = next_unprocessed()) s += c;

    put_back(c);

    return s;
}