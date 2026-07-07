#include <catch2/catch_test_macros.hpp>

#define interpreter_impl
#include <interpreter.h>

#include <variant>
#include <string>
#include <cstdint>

using namespace std::literals;

struct success_t {
    std::int32_t value;
    explicit success_t(std::int32_t value)
        : value(value) {}
};
success_t make_success(std::int32_t value) {
    return success_t(value);
}

struct failure_t {
    explicit failure_t(){}
};
failure_t make_failure() {
    return failure_t();
}

struct eval_result_t {
    eval_result_t(success_t s)
        : data(s) {}
    eval_result_t(failure_t f)
        : data(f) {}
    bool match_success(std::int32_t& out) const {
        if (auto* p = std::get_if<success_t>(&data)) {
            out = p->value;
            return true;
        }
        return false;
    }
    bool match_failure() const {
        return std::get_if<failure_t>(&data);
    }
private:
    std::variant<success_t, failure_t> data;
};

eval_result_t eval_case(const std::string& case_text) {
    std::int32_t res = 0;
    if(eval_program(case_text, res)) {
        return make_success(res);
    }
    return make_failure();
}

bool eval_case_fails(const std::string& case_text) {
    return eval_case(case_text).match_failure();
}

bool eval_case_equals(const std::string& case_text, std::int32_t value) {
    std::int32_t res;
    return eval_case(case_text).match_success(res) && res == value;
}

TEST_CASE("Test if string interner intern strings right", "[interner]") {
    string_interner_t interner;
    auto id_1 = interner.intern("foo");
    auto id_2 = interner.intern("bar");
    auto id_3 = interner.intern("buzz");
    REQUIRE( id_1 == 0 );
    REQUIRE( id_2 == 1 );
    REQUIRE( id_3 == 2 );

    REQUIRE(interner.intern("foo") == id_1);
    REQUIRE(interner.intern("buzz") == id_3);
    REQUIRE(interner.intern("bar") == id_2);
}

TEST_CASE("Incorrect programs do not work", "[eval]") {
    CHECK( eval_case_fails("["s) );
    CHECK( eval_case_fails("+"s) );
    CHECK( eval_case_fails("-"s) );
    CHECK( eval_case_fails("."s) );
    CHECK( eval_case_fails("<"s) );
    CHECK( eval_case_fails("*"s) );
    CHECK( eval_case_fails("/"s) );
    CHECK( eval_case_fails("]"s) );
    CHECK( eval_case_fails("2 2 + +"s) );
    CHECK( eval_case_fails("2 0 /"s) );
    CHECK( eval_case_fails("[ ]"s) );
}

TEST_CASE("Correct programs are working as expected", "[eval]"){
    CHECK( eval_case_equals("67"s, 67) );
    CHECK( eval_case_equals("67 42 swap"s, 67) );
    CHECK( eval_case_equals("[ ] 42"s, 42) );
    CHECK( eval_case_equals("42 [ ] swap"s, 42) );
    CHECK( eval_case_equals("1 1 +"s, 2) );
    CHECK( eval_case_equals("2 dup +"s, 4) );
    CHECK( eval_case_equals("[ :n n [ n dec odd ] [ 1 ] ifelse ] :even [ :n n [ n dec even ] [ 0 ] ifelse ] :odd 42 even"s, 1) );
    CHECK( eval_case_equals("[ :n n 2 < [ 1 ] [ n dup dec fact * ] ifelse ] :fact 5 fact"s, 120) );
    CHECK( eval_case_equals("[ :n 0 n [ dup inc + ] loop inc ] :pow2 5 pow2", 32) );
    CHECK( eval_case_equals("0 :a 1 10 [ a i + !a ] for a", 55) );
    CHECK( eval_case_equals("[ :count 0 :a 1 :b count [ b dup a + !b !a ] loop b ] :fib 6 fib", 13) );
}
