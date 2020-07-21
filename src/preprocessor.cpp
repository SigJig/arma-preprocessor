
#include "preprocessor.h"

#include <exception>
#include <ctype.h>
#include <algorithm>
#include <unordered_map>

static std::unordered_map<std::string, std::function<bool(char)>> validators {
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

macro::node::node(std::vector<std::string> args, macro* parent, preprocessor* pp)
    : m_args(args), m_parent(parent), m_pp(pp)
{
    m_stream = std::make_unique<std::stringstream>(m_parent->get_content());
}

macro::node macro::make_node(std::vector<std::string> args, preprocessor* pp)
{
    if (args.size() != m_args.size())
    {
        // throw exception
    }

    return node(args, this, pp);
}


char preprocessor::node::next()
{
    char c;

    if (!m_out_queue.empty())
    {
        c = m_out_queue.front();
        m_out_queue.pop();
    }
    else
    {
        c = next_processed();
    }

    return c;
}

char preprocessor::node::next_raw()
{
    char c;

    if (!m_in_queue.empty())
    {
        c = m_in_queue.front();
        m_in_queue.pop();
    }
    else
    {
        c = m_stream->get();
    }

    return c;
}

char preprocessor::node::next_unprocessed()
{
    return next_raw();
}

void preprocessor::node::put_back(char c)
{
    m_in_queue.push(c);
}

void preprocessor::node::fast_track(char c)
{
    m_out_queue.push(c);
}

std::string preprocessor::node::make_string(std::function<bool(char)> validator)
{
    std::string s;
    
    char c = next_unprocessed();
    for (; validator(c); c = next_unprocessed()) s += c;

    put_back(c);

    return s;
}

/*preprocessor::preprocessor(std::istream* stream)
{
    m_stream = std::make_unique<std::istream>(stream);
}*/

preprocessor::node::node(std::unique_ptr<std::istream> stream)
{
    m_stream = std::move(stream);
}

char preprocessor::node::next_unprocessed()
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

char preprocessor::node::next()
{
    char c = next_unprocessed();

    if (!c  || m_block_status == BLOCKED_BY_USER)
    {
        // TODO: If c is invalid it should do something idek
        return c;
    }
    else if (c == '"')
    {
        m_block_status = (m_block_status == STRING) ? UNBLOCKED : STRING;
        return c;
    }
    else if (c == '#')
    {
        std::string instruction = make_string(validators["identifier"]);
        std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        auto macros = m_pp->get_macros();

        if (instruction == "define")
        {
            std::string mac_name = make_string(validators["identifier"]);
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

            m_pp->add_macro(mac_name, macro(mac_name, args, content));
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

            std::string macro = make_string(validators["identifier"]);
            std::transform(macro.begin(), macro.end(), macro.begin(), ::tolower);

            bool is_defined = m_pp->find_macro(macro) != macros.end();
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
            std::string mac = make_string(validators["identifier"]);
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);

            m_pp->remove_macro(mac);
        }
    }
    else if (c == '_' || isalpha(c))
    {
        std::string identifier = make_string(validators["identifier"]);

        // Keep a seperate variable for the lowered identifer, so that if
        // it is not a macro we can put back the original characters.
        std::string lowered = identifier;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

        const auto mac = m_pp->find_macro(lowered);
        auto macros = m_pp->get_macros();

        if (mac != macros.end())
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

            m_pp->add_node(
                std::make_shared<node>(mac->second.make_node(args, m_pp))
            );
        }
        else
        {
            for (auto c : identifier) fast_track(c);
        }
    }

    return c;
}
