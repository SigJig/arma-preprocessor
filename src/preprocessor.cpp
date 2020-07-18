
#include "preprocessor.h"

#include <exception>
#include <ctype.h>
#include <algorithm>
#include <unordered_map>

static std::unordered_map<std::string, std::function<bool(char)>> callbacks {
    {"identfier", [](char c) -> bool {
        static bool is_first = true;

        if (is_first)
        {
            is_first = false;
            return c == '_' || isalpha(c);
        }

        return c == '_' || isalnum(c);
    }}
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
                fast_track(c_n);
            }
            else
            {
                put_back(c_n);
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
        std::string instruction = sequence(callbacks["identifier"]);
        std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        if (instruction == "define")
        {
            std::string macro = sequence(callbacks["identifier"]);
            std::vector<std::string> args;

            char c_n = next_char();

            if (c_n == '(')
            {
                c_n = next_char();

                while (c_n != ')')
                {
                    std::string arg;
                    for (; c_n != ',' && c_n != ')'; c_n = next_char())
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
            
            std::string content = sequence([](char c) -> bool {
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

            m_macros.emplace(macro, (macro, args, content));
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

            std::string macro = sequence(callbacks["identifier"]);
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
            std::string macro = sequence(callbacks["identifier"]);
            std::transform(macro.begin(), macro.end(), macro.begin(), ::tolower);

            m_macros.erase(macro);
        }
    }
    else if (c == '_' || isalpha(c))
    {
        std::string identifier = sequence(callbacks["identifier"]);

        // Keep a seperate variable for the lowered identifer, so that if
        // it is not a macro we can put back the original characters.
        std::string lowered = identifier;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

        const auto mac = m_macros.find(lowered);

        if (mac != m_macros.end())
        {
            std::vector<std::string > args;

            char c_n = next_char();

            if (c_n == '(')
            {
                while (next_char() != ')')
                {
                    args.push_back(sequence([](char c) -> bool {
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

char preprocessor::handle_block(char c)
{
    char c_n;

    if (m_block_status == STRING && c == '"')
    {
        m_block_status = UNBLOCKED;
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
            put_back(c_n);
        }
        else
        {
            fast_track(c_n);
            m_block_status = UNBLOCKED;
        }
    }

    return c;
}

char preprocessor::next_char(int char_exclude)
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

    // TODO: Should probably be iterative as it can overflow the stack
    if (char_exclude & SPACE && c == ' ')
    {
        return next_char(char_exclude);
    }
    else if (char_exclude & ALL_WHITESPACE && isspace(c))
    {
        return next_char(char_exclude);
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

void preprocessor::put_back(char c)
{
    m_in_queue.push(c);
}

void preprocessor::fast_track(char c)
{
    m_out_queue.push(c);
}

std::string preprocessor::sequence(std::function<bool(char)> callback)
{
    std::string s;
    
    char c = next_char();
    for (; callback(c); c = next_char()) s += c;
    
    put_back(c);

    return s;
}