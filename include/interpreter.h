#pragma once

#include <cstddef>
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

/// A range used to store info about words which are
/// dynamically recorded during the work of a program
struct word_range_t {
    std::size_t start;
    std::size_t end;
};

/// A tagged union for values that may be stored in a stack
struct value_t {
    value_t(std::int32_t n);
    value_t(const word_range_t& w);

    /// if matches number, places a number value in out
    /// and returns true, otherwise returns false
    bool matches_number(std::int32_t& out) const;

    /// if matches word, clears out, then fills it with ops from value
    /// and returns true, otherwise returns false
    bool matches_word_range(word_range_t& out) const;
private:
    std::variant<std::int32_t, word_range_t> data;
};

/// A stack of current values
using interpreter_stack_t = std::vector<value_t>;

/// An entry in a stack of words
struct word_entry_t { std::int32_t key; word_range_t range; };

/// A stack of words
using word_stack_t = std::vector<word_entry_t>;

/// A helper type which turns unique strings into unique identifiers
struct string_interner_t {
    string_interner_t();
    std::int32_t intern(std::string_view sv);
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
    word_t program_word;
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
    std::size_t stack_size_to_restore;
    std::size_t word_size_to_restore;
};

/// A function which parses a program, then interpret it, and stores
/// the result in res in the case of successful execution
bool eval_program(const std::string& program_text, std::int32_t& res);

/// A function which interpret a sequence of operations from the word
bool eval_word(interpreter_context_t& context, word_range_t range);

/// A function which interpret an operation from the word
bool eval_operation(const operation_t& op, interpreter_context_t& context);

/// A function that tries to parse a single operation
bool try_parse_operation(std::string_view sv, operation_t& op, interpreter_context_t& context);

/// A function that tries to parse a word of operations
void try_parse_word(const std::string& s, word_t& out_word, bool& success, interpreter_context_t& context);

#ifdef interpreter_impl

#include <iostream>

#define LOG_ERR(message) do { \
        std::cerr \
            << __LINE__  << ':' \
            << message << std::endl; \
    } while(0)

value_t::value_t(std::int32_t n)
    : data(n) {}

value_t::value_t(const word_range_t& w)
    : data(w) {}

bool value_t::matches_number(std::int32_t& out) const {
    if (auto* p = std::get_if<std::int32_t>(&data)) {
        out = *p;
        return true;
    }
    return false;
}

bool value_t::matches_word_range(word_range_t& out) const {
    if (auto* p = std::get_if<word_range_t>(&data)) {
        out = *p;
        return true;
    }
    return false;
}

string_interner_t::string_interner_t()
    : interned_strings(std::vector<std::string>{}) {}

std::int32_t string_interner_t::intern(std::string_view sv) {
    for (std::size_t i = 0; i < interned_strings.size(); i++)
        if (interned_strings[i] == sv)
            return (std::int32_t) i;

    auto res_id = (std::int32_t) interned_strings.size();
    interned_strings.emplace_back(sv);
    return res_id;
}

const std::string& string_interner_t::get_interned_string(std::int32_t id) const {
    static const std::string STRING_MISSING = "[[[STRING IS MISSING]]]";
    return id >= 0 && ((std::size_t) id < interned_strings.size())
        ? interned_strings[(std::size_t) id]
        : STRING_MISSING;
}

interpreter_context_t::interpreter_context_t()
    : word_stack(word_stack_t{}),
    stack(interpreter_stack_t{}),
    word_recorder_nesting(0),
    program_word({}),
    recording_word(word_t{}),
    interner(string_interner_t{}) {}

word_stack_guard_t::word_stack_guard_t(interpreter_context_t& ctx)
    : ctx(ctx),
    stack_size_to_restore(ctx.word_stack.size()),
    word_size_to_restore(ctx.program_word.size()) {}

word_stack_guard_t::~word_stack_guard_t() {
    while(ctx.word_stack.size() > stack_size_to_restore)
        ctx.word_stack.pop_back();
    while(ctx.program_word.size() > word_size_to_restore)
        ctx.program_word.pop_back();
}

bool is_all_digits(std::string_view sv, std::int32_t& payload, [[ maybe_unused ]] interpreter_context_t& context) {
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
bool is_call_word(std::string_view sv, std::int32_t& payload, interpreter_context_t& context) {
    payload = context.interner.intern(sv);
    return true;
}

bool is_store_word(std::string_view sv, std::int32_t& payload, interpreter_context_t& context) {
    if (sv.size() < 2 || sv[0] != ':')
        return false;

    auto sub_sv = sv.substr(1, sv.size() - 1);
    return is_call_word(sub_sv, payload, context);
}

bool is_push_word(std::string_view sv, std::int32_t& payload, interpreter_context_t& context) {
    if (sv.size() < 2 || sv[0] != '&')
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
    X_CALL(PushWord, is_push_word, eval_push_word ) \
    X_CALL(CallWord, is_call_word, eval_call_word)

enum class operation_tag_t {
    #define OP_CASE(name, _, __) name,
        OP_CASES(OP_CASE, OP_CASE)
    #undef OP_CASE
};

bool try_pop_number(interpreter_context_t& ctx, std::int32_t& out) {
    if (ctx.stack.empty()) return false;
    if (!ctx.stack.back().matches_number(out)) return false;
    ctx.stack.pop_back();
    return true;
}

bool try_pop_word(interpreter_context_t& ctx, word_range_t& out) {
    if (ctx.stack.empty()) return false;
    if (!ctx.stack.back().matches_word_range(out)) return false;
    ctx.stack.pop_back();
    return true;
}

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

    std::int32_t lhs, rhs;

    if (!(try_pop_number(context, rhs) && try_pop_number(context, lhs))) {
        LOG_ERR("Error: Expected two numers on top of a stack");
        return false;
    }

    context.stack.push_back(lhs + rhs);
    return true;
}

bool eval_sub(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Sub, payload);
        return true;
    }

    std::int32_t lhs, rhs;

    if (!(try_pop_number(context, rhs) && try_pop_number(context, lhs))) {
        LOG_ERR("Error: Expected two numers on top of a stack");
        return false;
    }

    context.stack.push_back(lhs - rhs);
    return true;
}

bool eval_mul(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Mul, payload);
        return true;
    }

    std::int32_t lhs, rhs;

    if (!(try_pop_number(context, rhs) && try_pop_number(context, lhs))) {
        LOG_ERR("Error: Expected two numers on top of a stack");
        return false;
    }

    context.stack.push_back(lhs * rhs);
    return true;
}

bool eval_div(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Div, payload);
        return true;
    }

    std::int32_t lhs, rhs;

    if (!(try_pop_number(context, rhs) && try_pop_number(context, lhs))) {
        LOG_ERR("Error: Expected two numers on top of a stack");
        return false;
    }

    if (rhs == 0) {
        LOG_ERR("Error: Division by zero");
        return false;
    }

    context.stack.push_back(lhs / rhs);
    return true;
}

bool eval_less_than(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::LessThan, payload);
        return true;
    }

    std::int32_t lhs, rhs;

    if (!(try_pop_number(context, rhs) && try_pop_number(context, lhs))) {
        LOG_ERR("Error: Expected two numers on top of a stack");
        return false;
    }

    context.stack.push_back(lhs < rhs ? 1 : 0);
    return true;
}

bool eval_print(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::Print, payload);
        return true;
    }

    std::int32_t x;
    if (!try_pop_number(context, x)) {
        LOG_ERR("Error: Expected a number on top of a stack");
        return false;
    }

    std::cout << x << std::endl;

    return true;
}

bool eval_if_else(std::int32_t payload, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::IfElse, payload);
        return true;
    }

    word_range_t else_word_range{0, 0}, then_word_range{0, 0};
    if (!(try_pop_word(context, else_word_range) && try_pop_word(context, then_word_range))) {
        LOG_ERR("Error: Expected a word for else and a word for then on top of a stack");
        return false;
    }

    std::int32_t cond;
    if (!try_pop_number(context, cond)) {
        LOG_ERR("Error: Expected a number on top of a stack");
        return false;
    }

    return cond
        ? eval_word(context, then_word_range)
        : eval_word(context, else_word_range);
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
        LOG_ERR("Error: Tried to decrease word recoder nesting while it is less than one");
        return false;
    }

    --context.word_recorder_nesting;
    if (context.word_recorder_nesting == 0) {
        // In the case of empty recording_word end will be less than start
        // which is completely fine, because such a word will never
        // be processed, and we needed exactly this kind of behaviour
        word_range_t new_word_range(
            context.program_word.size(),
            context.program_word.size() + context.recording_word.size() - 1
        );

        context.stack.push_back(new_word_range);
        for(auto op : context.recording_word)
            context.program_word.push_back(op);
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

    if (std::int32_t num; try_pop_number(context, num)) {
        word_range_t word{ context.program_word.size(), context.program_word.size() };
        context.program_word.emplace_back(operation_tag_t::Push, num);
        context.word_stack.emplace_back(word_id, word);
        return true;
    }

    if (word_range_t word_range{0, 0}; try_pop_word(context, word_range)) {
        context.word_stack.emplace_back(word_id, word_range);
        return true;
    }

    LOG_ERR("Error: Expected a number or a word on a top of a stack");
    return false;
}

bool eval_push_word(std::int32_t word_id, interpreter_context_t& context) {
    if (context.word_recorder_nesting > 0) {
        context.recording_word.emplace_back(operation_tag_t::PushWord, word_id);
        return true;
    }

    for (std::size_t i = 0; i < context.word_stack.size(); i++) {
        auto idx = context.word_stack.size() - 1 - i;
        const word_entry_t& word_entry = context.word_stack[idx];
        if (word_entry.key == word_id) {
            word_range_t word_range = word_entry.range;
            context.stack.push_back(word_range);
            return true;
        }
    }

    LOG_ERR("Error: a word '" << context.interner.get_interned_string(word_id) << "' not found in a word stack");
    return false;
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
            if (!eval_word(context, word_entry.range))
                return false;

            return true;
        }
    }

    LOG_ERR("Error: a word '" << context.interner.get_interned_string(word_id) << "' not found in a word stack");
    return false;
}

bool eval_word(interpreter_context_t& context, word_range_t range) {
    word_stack_guard_t guard(context);

    for (std::size_t i = range.start; i <= range.end; ++i) {
        auto op = context.program_word[i];
        if (!eval_operation(op, context))
            return false;
    }
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

bool try_parse_operation(std::string_view sv, operation_t& op, interpreter_context_t& context) {
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

void try_parse_word(const std::string& s, word_t& out_word, bool& success, interpreter_context_t& context) {
    word_t word;
    std::size_t start = 0;
    do {
        while(start < s.size() && is_whitespace(s[start]))
            ++start;

        if (start == s.size()) {
            success = true;
            out_word.clear();
            out_word.reserve(word.size());
            for(auto op : word)
                out_word.push_back(op);
            return;
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
        return;
    } while(start < s.size());

    success = true;
    out_word.clear();
    out_word.reserve(word.size());
    for(auto op : word)
        out_word.push_back(op);
}

bool eval_program(const std::string& program_text, std::int32_t& res) {
    static const std::string prologue =
        "[ :a a a ] :dup "
        "[ :a ] :drop "
        "[ :b :a b a ] :swap "
        "[ :b :a a b a ] :over "
        "[ 0 swap - ] :neg "
        "[ 1 + ] :inc "
        "[ 1 - ] :dec "
        "[ :body :count 0 count < [ body count dec &body loop ] [ ] ifelse ] :loop "
    ;

    interpreter_context_t context;

    bool successful_parse = false;
    word_t word;
    try_parse_word(prologue + program_text, word, successful_parse, context);
    if (!successful_parse) {
        LOG_ERR("Error: Failed to parse a program");
        return false;
    }

    if (word.empty()) {
        LOG_ERR("Error: Program is empty");
        return false;
    }

    context.program_word = word;

    if (!eval_word(context, {0, context.program_word.size()-1})) {
        LOG_ERR("Error: Program evaluation failed");
        return false;
    }

    if (context.stack.empty()) {
        LOG_ERR("Error: resulting stack is empty, expected at leas one number on top");
        return false;
    }

    std::int32_t num;
    if (!context.stack.back().matches_number(num)) {
        LOG_ERR("Error: Expected a number on a top of a stack as a result of a program");
        return false;
    }

    res = num;
    return true;
}

#endif
