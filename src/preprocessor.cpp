
#include "preprocessor.h"

#include <exception>
#include <ctype.h>
#include <algorithm>
#include <unordered_map>
#include <fstream>

// returns a validator function, so that we can use static inside that without
// affecting the function for later use
static std::function<bool(char)> iden_validator()
{
    return [](char c) -> bool {
        static bool is_first = true;

        if (is_first)
        {
            is_first = false;
            return c == '_' || isalpha(c);
        }

        return c == '_' || isalnum(c);
    };
}

node::node(preprocessor* pp)
    : m_pp(pp)
{}

node::node(std::shared_ptr<std::istream> stream)
{
    m_stream = stream;
}

char node::next()
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

char node::next_raw()
{
    char c;

    if (!m_in_queue.empty())
    {
        c = m_in_queue.front();
        m_in_queue.pop();
    }
    else
    {
        if (!m_stream->get(c)) return 0;
    }

    return c;
}

void node::put_back(char c)
{
    m_in_queue.push(c);
}

void node::fast_track(char c)
{
    m_out_queue.push(c);
}

char node::next_unprocessed()
{
    return next_raw();
}

std::string node::make_string(std::function<bool(char)> validator, bool skip_leading_ws)
{
    std::string s;
    
    char c = next_unprocessed();
    bool is_leading = true;
    for (;; c = next_unprocessed())
    {
        if (is_leading)
        {
            if (skip_leading_ws && isspace(c))
            {
                continue;
            }
            else
            {
                is_leading = false;
            }
        }

        if (!validator(c)) break;

        s += c;
    }

    put_back(c);

    return s;
}

preprocessor::preprocessor()
{  }

preprocessor::preprocessor(std::shared_ptr<std::istream> stream)
{
    add_node(std::make_shared<proc_node>(stream, this));
}

char preprocessor::next()
{
    while (true)
    {
        if (!m_nodes.size()) return 0;

        auto n = m_nodes.top();
        char c = n->next();

        if (c != 0)
        {
            return c;
        }
        else
        {
            m_nodes.pop();
        }
    }
}

void preprocessor::add_node(std::shared_ptr<node> n)
{
    m_nodes.push(n);
}

std::unordered_map<std::string, macro>& preprocessor::get_macros() { return m_macros; }

void preprocessor::add_macro(std::string name, macro m) {
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    m_macros.emplace(name, m);
}

preprocessor::proc_node::proc_node(std::shared_ptr<std::istream> stream, preprocessor* pp)
    : node::node(pp)
{
    m_stream = std::move(stream);
}

char preprocessor::proc_node::next_unprocessed()
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

char preprocessor::proc_node::next_processed()
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
    else if (m_block_status == STRING)
    {
        return c;
    }
    else if (c == '#')
    {
        std::string instruction = make_string(iden_validator());
        std::transform(instruction.begin(), instruction.end(), instruction.begin(), ::tolower);

        auto macros = m_pp->get_macros();

        if (instruction == "define")
        {
            std::string mac_name = make_string(iden_validator());
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
            std::string s = make_string([](char c) -> bool {
                static bool is_first = true;
                static bool still_looking = true;

                if (is_first)
                {
                    is_first = false;
                    return true;
                }
                else if (c == '"')
                {
                    still_looking = false;
                    
                    return true;
                }

                return still_looking;
            });

            s = s.substr(1, s.size() - 2);

            proc_node node(std::make_shared<std::ifstream>(s), m_pp);

            m_pp->add_node(std::make_shared<proc_node>(node));
        }
        else if (instruction == "ifdef" || instruction == "ifndef")
        {
            if (m_control_state != CLEAR)
            {
                throw std::invalid_argument("Unexpected " + instruction + ", already inside control statement");
            }

            std::string macro = make_string(iden_validator());
            std::transform(macro.begin(), macro.end(), macro.begin(), ::tolower);

            bool is_defined = macros.find(macro) != macros.end();
            bool truthy = instruction == "ifdef" ? is_defined : !is_defined;

            m_control_state = IFSTMT | (truthy ? 0 : BLOCKED);
        }
        else if (instruction == "else")
        {
            if (m_control_state & IFSTMT)
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
            std::string mac = make_string(iden_validator());
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);

            m_pp->get_macros().erase(mac);
        }

        return ' ';
    }
    else if (c == '_' || isalpha(c))
    {
        std::string identifier = c + make_string(iden_validator(), false);

        // Keep a seperate variable for the lowered identifer, so that if
        // it is not a macro we can put back the original characters.
        std::string lowered = identifier;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

        const auto macros = m_pp->get_macros();
        auto mac = macros.find(lowered);

        if (mac != macros.end())
        {
            std::vector<std::string> args;

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
                mac->second.make_node(args, m_pp)
            );
        }
        else
        {
            for (auto c : identifier) fast_track(c);
        }

        return ' ';
    }

    return c;
}


macro::macro(std::string name, std::vector<std::string> args, std::string content)
    : m_name(name), m_args(args), m_content(content)
{  }

std::shared_ptr<macro::macro_node> macro::make_node(std::vector<std::string> args, preprocessor* pp) const
{
    if (args.size() != m_args.size())
    {
        // throw exception
    }

    return std::make_shared<macro_node>(args, this, pp);
}

macro::macro_node::macro_node(preprocessor* pp)
    : node::node(pp)
{  }

macro::macro_node::macro_node(std::vector<std::string> args, const macro* parent, preprocessor* pp)
    : m_args(args), m_parent(parent), node::node(pp)
{
    m_stream = std::make_shared<std::stringstream>(m_parent->get_content());
}

char macro::macro_node::next_processed()
{
    return next_unprocessed();
}