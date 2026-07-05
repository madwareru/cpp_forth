#pragma once

#include <vector>
#include <variant>
#include <string>
#include <string_view>
#include <cstdint>

/// An operation tag used to mark all primitive operations
enum class operation_tag_t;

/// An operation along with its payload
struct operation_t {
    operation_tag_t tag;
    std::int32_t payload;
};

/// A word composed of primitive operations
using word_t = std::vector<operation_t>;

/// A tagged union for values that may be stored in a stack
struct value_t {
    value_t(std::int32_t n);
    value_t(const word_t& w);

    /// if matches number, places a number value in out
    /// and returns true, otherwise returns false
    bool matches_number(std::int32_t& out) const;

    /// if matches word, clears out, then fills it with ops from value
    /// and returns true, otherwise returns false
    bool matches_word(word_t& out) const;
private:
    std::variant<std::int32_t, word_t> data;
};

/// A stack of current values
using interpreter_stack_t = std::vector<value_t>;

/// An entry in a stack of words
struct word_entry_t { std::int32_t key; word_t word; };

/// A stack of words
using word_stack_t = std::vector<word_entry_t>;

/// A helper type which turns unique strings into unique identifiers
struct string_interner_t {
    string_interner_t();
    std::int32_t intern(const std::string_view& sv);
    const std::string& get_interned_string(std::int32_t id) const;
private:
    // We may want to do something smarter later
    // Using hash map or may be even trie, but for
    // now the solution with a flat dynamic array is ok
    std::vector<std::string> interned_strings;
};

/// A context of an interpreter storing all the data needed for an interpretation
struct interpreter_context_t {
    word_stack_t word_stack;
    interpreter_stack_t stack;
    std::size_t word_recorder_nesting;
    word_t recording_word;
    string_interner_t interner;

    interpreter_context_t();
};

/// Scope guard to encapsulate word stack restoring
/// after execution of words, which may introduce new ones
struct word_stack_guard_t {
    word_stack_guard_t(interpreter_context_t& ctx);
    ~word_stack_guard_t();
private:
    interpreter_context_t& ctx;
    std::size_t size_to_restore;
};

/// A function which parses a program, then interpret it, and stores
/// the result in res in the case of successful execution
bool eval_program(const std::string& program_text, std::int32_t& res);

/// A function which interpret a sequence of operations from the word
bool eval_word(const word_t& word, interpreter_context_t& context);

/// A function which interpret an operation from the word
bool eval_operation(const operation_t& op, interpreter_context_t& context);

/// A function that tries to parse a single operation
bool try_parse_operation(const std::string_view& sv, operation_t& op, interpreter_context_t& context);

/// A function that tries to parse a word of operations
word_t try_parse_word(const std::string& s, bool& success, interpreter_context_t& context);

#ifdef interpreter_impl

#include <iostream>

value_t::value_t(std::int32_t n)
    : data(n) {}

value_t::value_t(const word_t& w)
    : data(w) {}

bool value_t::matches_number(std::int32_t& out) const {
    if (auto* p = std::get_if<std::int32_t>(&data)) {
        out = *p;
        return true;
    }
    return false;
}

bool value_t::matches_word(word_t& out) const {
    if (auto* p = std::get_if<word_t>(&data)) {
        out = *p;
        return true;
    }
    return false;
}

string_interner_t::string_interner_t()
    : interned_strings(std::vector<std::string>{}) {}

std::int32_t string_interner_t::intern(const std::string_view& sv) {
    for (std::size_t i = 0; i < interned_strings.size(); i++)
        if (interned_strings[i] == sv)
            return (std::int32_t) i;

    auto res_id = (std::int32_t) interned_strings.size();
    interned_strings.emplace_back(sv);
    return res_id;
}

const std::string STRING_MISSING = "[[[STRING IS MISSING]]]";

const std::string& string_interner_t::get_interned_string(std::int32_t id) const {
    return id >= 0 && ((std::size_t) id < interned_strings.size())
        ? interned_strings[(std::size_t) id]
        : STRING_MISSING;
}

interpreter_context_t::interpreter_context_t()
    : word_stack(word_stack_t{}),
    stack(interpreter_stack_t{}),
    word_recorder_nesting(0),
    recording_word(word_t{}),
    interner(string_interner_t{}) {}

word_stack_guard_t::word_stack_guard_t(interpreter_context_t& ctx)
    : ctx(ctx), size_to_restore(ctx.word_stack.size()) {}

word_stack_guard_t::~word_stack_guard_t() {
    while(ctx.word_stack.size() > size_to_restore)
        ctx.word_stack.pop_back();
}

bool is_all_digits(const std::string_view& sv, std::int32_t& payload, [[ maybe_unused ]] interpreter_context_t& context) {
    for (const char& c : sv)
        if (c < '0' || c > '9')
            return false;

    payload = 0;
    for (const char& c : sv)
        payload = payload * 10 + (c - '0');
    return true;
}

// Attention: this check is always true,
// so CallWord should be at the end of an enumeration
bool is_call_word(const std::string_view& sv, std::int32_t& payload, interpreter_context_t& context) {
    payload = context.interner.intern(sv);
    return true;
}

bool is_store_word(const std::string_view& sv, std::int32_t& payload, interpreter_context_t& context) {
    if (sv.size() < 2 || sv[0] != ':')
        return false;

    auto sub_sv = sv.substr(1, sv.size() - 1);
    return is_call_word(sub_sv, payload, context);
}

bool is_bind_to_word(const std::string_view& sv, std::int32_t& payload, interpreter_context_t& context) {
    if (sv.size() < 2 || sv[0] != '!')
        return false;

    auto sub_sv = sv.substr(1, sv.size() - 1);
    return is_call_word(sub_sv, payload, context);
}

#define OP_CASES(X_TOKEN, X_CALL) \
    X_CALL(Push, is_all_digits, eval_push) \
    X_TOKEN(Add, "+", eval_add) \
    X_TOKEN(Sub, "-", eval_sub) \
    X_TOKEN(Mul, "*", eval_mul) \
    X_TOKEN(Div, "/", eval_div) \
    X_TOKEN(LessThan, "<", eval_less_than) \
    X_TOKEN(Print, ".", eval_print) \
    X_TOKEN(IfElse, "ifelse", eval_if_else) \
    X_TOKEN(StartRecord, "[", eval_start_record) \
    X_TOKEN(EndRecord, "]", eval_end_record) \
    X_CALL(StoreWord, is_store_word, eval_store_word) \
    X_CALL(BindToWord, is_bind_to_word, eval_bind_to_word) \
    X_CALL(CallWord, is_call_word, eval_call_word)

enum class operation_tag_t {
    #define OP_CASE(name, _, __) name,
        OP_CASES(OP_CASE, OP_CASE)
    #undef OP_CASE
};

bool eval_push(std::int32_t num, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Push, num);
        return true;
    }
    context.stack.push_back(num);

    return true;
}

bool eval_add(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Add, payload);
        return true;
    }

    if (context.stack.size() < 2) {
        std::cerr << "Error: trying to get a sum of two values, while stack size is less than 2" << std::endl;
        return false;
    }

    std::int32_t lhs, rhs;

    if (!context.stack.back().matches_number(rhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    if (!context.stack.back().matches_number(lhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.stack.push_back(lhs + rhs);
    return true;
}

bool eval_sub(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Sub, payload);
        return true;
    }

    if (context.stack.size() < 2) {
        std::cerr << "Error: trying to get a sub of two values, while stack size is less than 2" << std::endl;
        return false;
    }

    std::int32_t lhs, rhs;

    if (!context.stack.back().matches_number(rhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    if (!context.stack.back().matches_number(lhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.stack.push_back(lhs - rhs);
    return true;
}

bool eval_mul(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Mul, payload);
        return true;
    }

    if (context.stack.size() < 2) {
        std::cerr << "Error: trying to get a mul of two values, while stack size is less than 2" << std::endl;
        return false;
    }

    std::int32_t lhs, rhs;

    if (!context.stack.back().matches_number(rhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    if (!context.stack.back().matches_number(lhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.stack.push_back(lhs * rhs);
        return true;
}

bool eval_div(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Div, payload);
        return true;
    }

    if (context.stack.size() < 2) {
        std::cerr << "Error: trying to get a div of two values, while stack size is less than 2" << std::endl;
        return false;
    }

    std::int32_t lhs, rhs;

    if (!context.stack.back().matches_number(rhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();
    if (rhs == 0) {
        std::cerr << "Error: Division by zero" << std::endl;
        return false;
    }

    if (!context.stack.back().matches_number(lhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.stack.push_back(lhs / rhs);
        return true;
}

bool eval_less_than(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::LessThan, payload);
        return true;
    }

    if (context.stack.size() < 2) {
        std::cerr << "Error: trying to compare two values, while stack size is less than 2" << std::endl;
        return false;
    }

    std::int32_t lhs, rhs;

    if (!context.stack.back().matches_number(rhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    if (!context.stack.back().matches_number(lhs)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.stack.push_back(lhs < rhs ? 1 : 0);
        return true;
}

bool eval_print(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Print, payload);
        return true;
    }

    if (context.stack.empty()) {
        std::cerr << "Error: trying to pop a value from an empty stack" << std::endl;
        return false;
    }

    std::int32_t x;
    if (!context.stack.back().matches_number(x)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    std::cout << x << std::endl;

    return true;
}

bool eval_if_else(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::IfElse, payload);
        return true;
    }

    if (context.stack.size() < 3) {
        std::cerr << "Error: Expected a stack size to be 3 or more" << std::endl;
        return false;
    }

    word_t else_word;
    if (!context.stack.back().matches_word(else_word)) {
        std::cerr << "Error: Expected a word on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    word_t then_word;
    if (!context.stack.back().matches_word(then_word)) {
        std::cerr << "Error: Expected a word on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    std::int32_t cond;
    if (!context.stack.back().matches_number(cond)) {
        std::cerr << "Error: Expected a number on top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    if (cond) {
        if (!eval_word(then_word, context))
            return false;
    } else {
        if (!eval_word(else_word, context))
            return false;
    }

    return true;
}

bool eval_start_record(std::int32_t payload, interpreter_context_t& context) {
    ++context.word_recorder_nesting;
    if (context.word_recorder_nesting > 1) {
        context.recording_word.emplace_back(operation_tag_t::StartRecord, payload);
    }
    return true;
}

bool eval_end_record(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting < 1) {
        std::cerr << "Error: Tried to decrease word recoder nesting while it is less than one" << std::endl;
        return false;
    }

    --context.word_recorder_nesting;
    if (context.word_recorder_nesting == 0) {
        context.stack.push_back(context.recording_word);
        context.recording_word.clear();
        return true;
    }

    context.recording_word.emplace_back(operation_tag_t::EndRecord, payload);
    return true;
}

bool eval_store_word(std::int32_t word_id, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::StoreWord, word_id);
        return true;
    }

    if (context.stack.empty()) {
        std::cerr << "Error: trying to store word while stack is empty" << std::endl;
        return false;
    }

    word_t word;
    if (!context.stack.back().matches_word(word)) {
        std::cerr << "Error: Expected a word on a top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    context.word_stack.emplace_back(word_id, word);
    return true;
}

bool eval_bind_to_word(std::int32_t word_id, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::BindToWord, word_id);
        return true;
    }

    if (context.stack.empty()) {
        std::cerr << "Error: trying to bind word while stack is empty" << std::endl;
        return false;
    }

    std::int32_t num;
    if (!context.stack.back().matches_number(num)) {
        std::cerr << "Error: Expected a number on a top of a stack" << std::endl;
        return false;
    }
    context.stack.pop_back();

    word_t word;
    word.emplace_back(operation_tag_t::Push, num);
    context.word_stack.emplace_back(word_id, word);

    return true;
}

bool eval_call_word(std::int32_t word_id, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::CallWord, word_id);
        return true;
    }

    for (std::size_t i = 0; i < context.word_stack.size(); i++) {
        auto idx = context.word_stack.size() - 1 - i;
        const word_entry_t& word_entry = context.word_stack[idx];
        if (word_entry.key == word_id) {
            if (!eval_word(word_entry.word, context))
                return false;

            return true;
        }
    }

    std::cerr
        << "Error: a word '" << context.interner.get_interned_string(word_id) << "' not found in a word stack"
        << std::endl;
    return false;
}

bool eval_word(const word_t& word, interpreter_context_t& context) {
    word_stack_guard_t guard(context);
    for (const operation_t& op : word)
        if (!eval_operation(op, context))
            return false;
    return true;
}

bool eval_operation(const operation_t& op, interpreter_context_t& context) {
    switch(op.tag) {
        #define OP_CASE(name, _, function) \
            case operation_tag_t::name: return function(op.payload, context);
        OP_CASES(OP_CASE, OP_CASE)
        #undef OP_CASE
        default: return false;
    }
}

bool try_parse_operation(const std::string_view& sv, operation_t& op, interpreter_context_t& context) {
    #define OP_CALL(name, predicate, _) \
        if (std::int32_t payload; predicate(sv, payload, context)) { \
            op = { operation_tag_t::name, payload }; \
            return true; \
        }
    #define OP_TOKEN(name, literal, _) \
        if (sv == literal) { \
            op = { operation_tag_t::name, 0 }; \
            return true; \
        }
    OP_CASES(OP_TOKEN, OP_CALL)
    #undef OP_TOKEN
    #undef OP_CALL
    return false;
}

bool is_whitespace(char ch) {
    return
        ch == ' ' ||
        ch == '\t' ||
        ch == '\n';
}

word_t try_parse_word(const std::string& s, bool& success, interpreter_context_t& context) {
    word_t word;
    std::size_t start = 0;
    do {
        while(start < s.size() && is_whitespace(s[start]))
            ++start;

        if (start == s.size()) {
            success = true;
            return word;
        }

        std::size_t pos = s.find_first_of(" \t\n", start);

        std::string_view sv;
        if (pos == std::string::npos) {
            sv = std::string_view(s).substr(start, s.size() - start);
            start = s.size();
        } else {
            sv = std::string_view(s).substr(start, pos - start);
            start = pos + 1;
        }

        if (operation_t op; try_parse_operation(sv, op, context)) {
            word.push_back(op);
            continue;
        }

        success = false;
        return word_t(0);
    } while(start < s.size());

    success = true;
    return word;
}

bool eval_program(const std::string& program_text, std::int32_t& res) {
    interpreter_context_t context;

    bool successful_parse = false;
    word_t word = try_parse_word(program_text, successful_parse, context);
    if (!successful_parse) {
        std::cerr << "Error: Failed to parse a program" << std::endl;
        return false;
    }

    if (!eval_word(word, context)) {
        std::cerr << "Error: Program evaluation failed" << std::endl;
        return false;
    }

    if (context.stack.empty()) {
        std::cerr << "Error: resulting stack is empty, expected at leas one number on top" << std::endl;
        return false;
    }

    std::int32_t num;
    if (!context.stack.back().matches_number(num)) {
        std::cerr << "Error: Expected a number on a top of a stack" << std::endl;
        return false;
    }

    res = num;
    return true;
}

#endif
