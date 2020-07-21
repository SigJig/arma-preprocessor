
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <queue>
#include <stack>
#include <functional>
#include <inttypes.h>
#include <string>
#include <map>
#include <vector>
#include <regex>
#include <memory>

class macro
{
public:
    macro(std::string name, std::vector<std::string> args, std::string content);

    std::string get_content() { return m_content; }

    class node : public preprocessor::node
    {
    public:
        node(std::vector<std::string> args, macro* parent, preprocessor* pp);
        node(const node&) = default;
        node(node&) = default;

    protected:
        char next_processed() { return 0; }

        std::vector<std::string> m_args;
        macro* m_parent;
    };

    node make_node(std::vector<std::string> args, preprocessor* pp);

protected:
    std::string m_name;
    std::vector<std::string> m_args;
    std::string m_content;
};


class preprocessor
{
public:
    class node
    {
    public:
        node(std::unique_ptr<std::istream> stream, preprocessor* pp);
        node(node& other) = default;
        node(const node& other) = default;

        char next();
    protected:
        enum block_t
        {
            UNBLOCKED = 0,
            STRING,
            SL_COMMENT,
            ML_COMMENT,
            BLOCKED_BY_USER
        };

        enum controlstate {
            CLEAR = 0,
            IFSTMT = 1 << 0,
            BLOCKED = 1 << 1
        };

        char next_raw();
        void put_back(char c); // Put a character back into the in queue
        void fast_track(char c); // Push a character directly to the out queue
    
        virtual char next_processed() = 0;
        virtual char next_unprocessed();

        std::string make_string(std::function<bool(char)> validator);

        std::unique_ptr<std::istream> m_stream;

        std::queue<char> m_in_queue;
        std::queue<char> m_out_queue;

        preprocessor* m_pp;

        // used for determining whether or not to preprocess the current character
        block_t m_block_status = UNBLOCKED;

        // used for determining whether or not to return the current character
        int m_control_state = CLEAR;
    };

    char next();
    void add_node(std::shared_ptr<node> n);

    void add_macro(std::string name, macro m);
    void remove_macro(std::string name);
    std::map<std::string, macro>::iterator find_macro(std::string name);

    std::map<std::string, macro> get_macros();

protected:

    std::map<std::string, macro> m_macros;
    std::stack<std::shared_ptr<node>> m_nodes;

    char next_unprocessed();
};

#endif // PREPROCESSOR_H