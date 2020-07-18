
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

class macro
{
public:
    macro(std::string name, std::vector<std::string> args, std::string content);

    class instance
    {
    public:
        instance(std::vector<std::string> args, macro* parent);

        char advance()
        {
            return 0;
        }

        char operator()() { return advance(); }

    protected:
        std::vector<std::string> m_args;
        macro* m_parent;
    };

    instance make_instance(std::vector<std::string> args);

protected:
    std::string m_name;
    std::vector<std::string> m_args;
    std::string m_content;
};

class preprocessor
{
public:
    typedef std::function<char()> reader_t;

    preprocessor(reader_t reader);

    char next_processed();
    char process(char c);

    void add_reader(const reader_t& reader);

protected:
    // false by default, true if preprocessing of characters is blocked for some reason.
    // for example, if in a string or a comment the characters will not be processed.
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

    // used for determining whether or not to preprocess the current character
    block_t m_block_status = UNBLOCKED;

    // used for determining whether or not to return the current character
    int m_control_state = CLEAR;

    std::queue<char> m_in_queue;
    std::queue<char> m_out_queue;

    char handle_block(char c);

    void put_back(char c); // Put a character back into the in queue
    void fast_track(char c); // Push a character directly to the out queue

    std::map<std::string, macro> m_macros;

    reader_t get_reader();

    std::stack<reader_t> m_readers;

    char next_unprocessed();
    char next_raw();

    std::string make_string(std::function<bool(char)> callback);
};

#endif // PREPROCESSOR_H