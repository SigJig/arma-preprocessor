
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <queue>
#include <stack>
#include <functional>
#include <inttypes.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <memory>

class preprocessor;

class node
{
public:
    node(preprocessor* pp);
    node(std::shared_ptr<std::istream> stream);

    char next();

protected:
    char next_raw();
    void put_back(char c); // Put a character back into the in queue
    void fast_track(char c); // Push a character directly to the out queue

    std::queue<char> m_in_queue;
    std::queue<char> m_out_queue;

    virtual char next_processed() = 0;
    virtual char next_unprocessed();

    std::string make_string(std::function<bool(char)> validator);
    std::shared_ptr<std::istream> m_stream;

    preprocessor* m_pp;
};


class macro
{
public:
    class macro_node : public node
    {
    public:
        macro_node(preprocessor* pp);
        macro_node(std::vector<std::string> args, const macro* parent, preprocessor* pp);

        char next_processed();
        
    protected:
        std::vector<std::string> m_args;
        const macro* m_parent;
    };

    macro(std::string name, std::vector<std::string> args, std::string content);

    std::string get_content() const { return m_content; }
    
    std::shared_ptr<macro::macro_node> make_node(std::vector<std::string> args, preprocessor* pp) const;
    
protected:
    std::string m_name;
    std::vector<std::string> m_args;
    std::string m_content;
};


class preprocessor
{
public:
    class proc_node : public node
    {
    public:
        proc_node(std::shared_ptr<std::istream> stream, preprocessor* pp);
        proc_node(proc_node& other) = default;
        proc_node(const proc_node& other) = default;
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


        char next_processed();
        char next_unprocessed();

        // used for determining whether or not to preprocess the current character
        block_t m_block_status = UNBLOCKED;

        // used for determining whether or not to return the current character
        int m_control_state = CLEAR;
    };

    preprocessor();
    preprocessor(std::shared_ptr<std::istream> stream);

    char next();
    void add_node(std::shared_ptr<node> n);

    std::unordered_map<std::string, macro>& get_macros();
    void add_macro(std::string name, macro m);

protected:
    std::stack<std::shared_ptr<node>> m_nodes;
    std::unordered_map<std::string, macro> m_macros;
};


#endif // PREPROCESSOR_H