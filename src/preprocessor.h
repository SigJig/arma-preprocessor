
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <iostream>
#include <queue>
#include <stack>
#include <functional>
#include <inttypes.h>
#include <string>
#include <map>
#include <vector>

enum charflag {
    ALPHA = 1 << 0,
    NUMERIC = 1 << 2,
    UNDERSCORE = 1 << 3
};

enum controlstate {
    CLEAR = 0,
    IFSTMT = 1 << 0,
    BLOCKED = 1 << 1
};

class macro
{
public:
    macro(std::string name, std::vector<std::string> args);

protected:
    std::vector<std::string> m_args;
};

class preprocessor
{
public:
    typedef std::function<char()> reader_t;

    preprocessor(reader_t reader);

    char next();
    char process(char c);

    void add_reader(reader_t& reader);

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

    block_t m_block_status = UNBLOCKED;
    int m_control_state = CLEAR;

    char handle_block(char c);

    std::queue<char> m_in_queue;
    std::queue<char> m_out_queue;

    std::map<std::string, macro> m_macros;

    reader_t get_reader();

    std::stack<reader_t> m_readers;

    char next_char();

    std::string get_sequence(int flag);
};

#endif // PREPROCESSOR_H