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

TEST_CASE("Incorrect programs do not work unexpectedly well", "[eval]") {
    REQUIRE( eval_case_fails("["s) );
    REQUIRE( eval_case_fails("+"s) );
    REQUIRE( eval_case_fails("-"s) );
    REQUIRE( eval_case_fails("."s) );
    REQUIRE( eval_case_fails("<"s) );
    REQUIRE( eval_case_fails("*"s) );
    REQUIRE( eval_case_fails("/"s) );
    REQUIRE( eval_case_fails("]"s) );
    REQUIRE( eval_case_fails("2 2 + +"s) );
    REQUIRE( eval_case_fails("2 0 /"s) );
}

TEST_CASE("Correct programs are working as expected", "[eval]"){
    REQUIRE( eval_case_equals("1 1 +"s, 2) );
    REQUIRE( eval_case_equals("2 dup +"s, 4) );
    REQUIRE( eval_case_equals("[ !n n 2 < [ 1 ] [ n n 1 - fact * ] ifelse ] :fact 5 fact"s, 120));
}
