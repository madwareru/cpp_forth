# CPP_FORTH

[![CMake on multiple platforms](https://github.com/madwareru/cpp_forth/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/madwareru/cpp_forth/actions/workflows/cmake-multi-platform.yml)

This project is inspired by a beautiful [fortik](https://github.com/true-grue/fortik/tree/main).

It’s essentially an interpreter of a very small dialect of the Forth language, which unfortunately lacks its own official name. You can call it FortikCpp if you want. Or any other name — it doesn’t really matter much to me. 

This language interpreter was written for educational purposes and also just for fun. 

There are no guarantees regarding the safety of the interpreter’s implementation, although I tried to be as careful as I can. Feel free to point out any issues in an issue tracker if you find examples of programs that will cause it to segmentation fault. 

## Quick introduction to the language

The language has a very small set of primitive operations, and these operations are written as sequences of printable characters separated by whitespace. These sequences of operations are known as words, and they are read strictly from left to right. 

The key property of a program written in FortikCpp is that the data for each operation is ready before the evaluation of this operation. This is possible thanks to the stack nature of the language and to the so‑called Reverse Polish Notation, where operands of operations are always written before the operation that operates on them. This property helps to write complex expressions without the need for grouping by brackets, and it also helps to write programs concatenatively. 

The places where the data for operations live are known as stacks, just like a stack of plates or a stack of playing cards. When someone wants to do something, they will first write commands to place some data on top of the stack, then run an operation. The operation, in turn, will take this data from the stack during its work, process it, and then store some result in the stack (or it might not store anything if no result is expected). 

There are a couple of stacks where data can be looked up:
1. The first stack is a stack of values, and it can store either a `number` or a `recorded word`.
2. The stack of words associated with names. These words are essentially an analogue of simple functions. 

### Pushing data to the value stack
Let's look how the data could be placed in these stacks. The first operation we need to look at 
is the `Push` operation, which gets a number and places it in the stack. 

To write this operation in a program, we just type several digits (negative number literals aren't supported):
```
10
```
Evaluation of such an operation will push the number `10` onto the stack which we will visualize as `[| <- 10]`,
where `|` is a bottom of a stack, and `<-` before the `10` symbolizes that the `10` lies on a top of a bottom.

If we then had a sequence of numbers after `10`, like that:
```
13 67 42
```
We would end up with a stack looking like `[| <- 10 <- 13 <- 67 <- 42]` after evaluation of a word.

Now let's take a look at a different word which is similar to the previous, but has tokens `[` and `]` inside:
```
10 [ 13 67 42 ]
```
Evaluation of such a word would turn our stack into `[| <- 10 <- <13 67 42> ]`. The `<13 67 42>` thing is a recorded 
word which ended up on a top of a stack. A token `[` marks a `StartRecord` operation, and a token `]` marks an 
`EndRecord` operation. Anything between those are being recorded and then eventually will be placed on a stack. 
There is a possibility of nested recordings, and in such situation tokens will end up recorded as well. 
For example, such a word:
```
10 [ 13 [ 67 ] 42 ]
```
Evaluating it will turn our stack into `[| <- 10 <- <13 [ 67 ] 42> ]`, so the nested recorders ending up recorded 
as is without any evaluation.

### Pushing data to the word stack
Now it's time to see how to bind a value or a word to a name.
First, let's look at the situation when we have a stack in a form of `[ ... <- <36 67 42> ]`. This can be read as a 
word `36 67 42` lying on a top of a stack, ignoring the fact if it is some data beneath or not.
Now, if we evaluate an operation which looks like any sequence of printable characters with a single character ':' 
before it (it has a name `StoreWord`), for example:
```
:foo
```
Evaluating this operation will turn the word stack into `[| <- (foo: <36 67 42>)]`, which can be read as a pair of key 
`foo` and a word `<36 67 42>` on top of the bottom of a stack.

The next example word is the following:
```
1990 :birth-year
```
Evaluating this word will first push the value `1990` on a value stack, then pop the int from a stack and turn an empty word stack into the `[| <- (birth-year: <1990>)]`, so as we can see, values are ending up boxed as a word with a single `Push` operation.

### Setting new value for a value
In the case when there is a value (a word with a single `Push` operation) on a value stack, we also support a `SetVar` operation which can replace a number which will be pushed during evaluation of a word, i.e. if we have a word stack of a form `[ ... <- (age: <35>) <- .. ]`, where `(age: <35>)` is the first occurrence of a binding to `age` on top of the stack, 
and if we also have a value stack of a form `[ ... <- 36 ]`, then if we evaluate an operation which looks like any sequence of printable characters with a single character '!' before it (and in our case it should be exactly `!age`), 
then the value `36` would be popped from a stack and a word stack would be modified so it will look exactly like 
`[ ... <- (age: <36>) <- .. ]`.

### Value stack modifier primitives
There are several standard operations which help to write useful programs in FortikCpp:
- `Dup` operation (duplicate) is written as `dup` and it turns stack of the form `[ ... <- $x]` 
  into a stack of the form `[ ... <- $x <- $x]`, meaning it will make an exact copy of the top of a stack on top 
  of a top of a stack.
- `Drop` operation is written as `drop` and it turns stack of the form `[ ... <- $x]` into a stack of the form 
  `[ ... ]`, meaning it literally drops the top of a stack.
- `Swap` operation is written as `swap` and it swaps two the values on a top of a stack, i.e. it turns stack of 
  the form `[ ... <- $x <- $y ]` into a stack of the form `[ ... <- $y <- $x ]`.
- `Over` operation is written as `over` and it places the copy of a value below the top of a stack on top of a 
  top of a stack, i.e. it turns stack of the form `[ ... <- $x <- $y ]` into a stack of the form 
  `[ ... <- $x <- $y <- $x ]`.
- `RotLeft` operation is written as `rot` and it turns stack of the form `[ ... <- $x <- $y <- $z ]` 
  into a stack of the form `[ ... <- $y <- $z <- $x ]`.
- `RotRight` operation is written as `rot-r` and it turns stack of the form `[ ... <- $x <- $y <- $z ]` 
  into a stack of the form `[ ... <- $z <- $x <- $y ]`.

### Arithmetic operations
There are four standard arithmetic operations that work on numbers on the value stack:

- `Add` operation is written as `+` and it pops two numbers from the top of a stack, adds them, 
  and pushes the result. It turns a stack of the form `[ ... <- $x <- $y ]` into `[ ... <- $x + $y ]`. 
  For example:
  ```
  10 20 +
  ```
  Evaluating this word would result in a stack of `[| <- 30]`.

- `Sub` operation is written as `-` and it pops two numbers from the top of a stack, subtracts 
  the second from the first, and pushes the result. It turns a stack of the form `[ ... <- $x <- $y ]` 
  into `[ ... <- $x - $y ]`. For example:
  ```
  50 15 -
  ```
  Evaluating this word would result in a stack of `[| <- 35]`.

- `Mul` operation is written as `*` and it pops two numbers from the top of a stack, multiplies them, 
  and pushes the result. It turns a stack of the form `[ ... <- $x <- $y ]` into `[ ... <- $x * $y ]`. 
  For example:
  ```
  6 7 *
  ```
  Evaluating this word would result in a stack of `[| <- 42]`.

- `Div` operation is written as `/` and it pops two numbers from the top of a stack, 
  divides the first by the second, and pushes the result. It turns a stack of the 
  form `[ ... <- $x <- $y ]` into `[ ... <- $x / $y ]`. For example:
  ```
  100 4 /
  ```
  Evaluating this word would result in a stack of `[| <- 25]`. Note that dividing by zero will cause an error.

In addition, there are three standard words implemented using primitive operations that behave as unary operations:

- `neg` (negate) pops a number from the top of a stack and pushes its negated value. 
  It turns a stack of the form `[ ... <- $x ]` into `[ ... <- -$x ]`. For example:
  ```
  42 neg
  ```
  Evaluating this word would result in a stack of `[| <- -42]`.

- `inc` (increment) pops a number from the top of a stack and pushes the value increased by one. 
  It turns a stack of the form `[ ... <- $x ]` into `[ ... <- $x + 1 ]`. For example:
  ```
  7 inc
  ```
  Evaluating this word would result in a stack of `[| <- 8]`.

- `dec` (decrement) pops a number from the top of a stack and pushes the value decreased by one. 
  It turns a stack of the     form `[ ... <- $x ]` into `[ ... <- $x - 1 ]`. For example:
  ```
  7 dec
  ```
  Evaluating this word would result in a stack of `[| <- 6]`.

### Comparison operations
There is one primitive comparison operation that works on numbers on the value stack, and
three standard words implemented using it. All comparison operations return `-1` for true and
`0` for false, which aligns with the truthiness convention used by control flow operations
(non-zero is truthy, zero is falsy).

- `LessThan` operation is written as `<` and it pops two numbers from the top of a stack,
  compares them, and pushes the result. It turns a stack of the form `[ ... <- $x <- $y ]`
  into `[ ... <- ($x < $y ? -1 : 0) ]`. For example:
  ```
  3 5 <
  ```
  Evaluating this word would result in a stack of `[| <- -1]`, because 3 is less than 5.

In addition, there are three standard words implemented using the `LessThan` primitive:

- `>` (greater-than) is defined as `[ swap < ] :>` and it pops two numbers, swaps them, and
  applies `<`. It turns a stack of the form `[ ... <- $x <- $y ]` into
  `[ ... <- ($x > $y ? -1 : 0) ]`. For example:
  ```
  5 3 > 
  ```
  Evaluating this word would result in a stack of `[| <- -1]`, because 5 is greater than 3.

- `>=` (greater-or-equal) is defined as `[ < [ 0 ] [ 1 neg ] ifelse ] :>=` and it pops two
  numbers, applies `<`, and then uses `ifelse` (we will cover this operation in a next section) 
  to return `-1` when the result is falsy (i.e.

  when the values are not less-than, meaning greater-or-equal) and `0` otherwise. It turns a
  stack of the form `[ ... <- $x <- $y ]` into `[ ... <- ($x >= $y ? -1 : 0) ]`. For example:
  ```
  5 5 >=
  ```
  Evaluating this word would result in a stack of `[| <- -1]`, because 5 is greater than or
  equal to 5.

- `<=` (less-or-equal) is defined as `[ > [ 0 ] [ 1 neg ] ifelse ] :<=` and it pops two
  numbers, applies `>`, and then uses `ifelse` to return `-1` when the result is falsy (i.e.
  when the values are not greater-than, meaning less-or-equal) and `0` otherwise. It turns a
  stack of the form `[ ... <- $x <- $y ]` into `[ ... <- ($x <= $y ? -1 : 0) ]`. For example:
  ```
  3 5 <=
  ```
  Evaluating this word would result in a stack of `[| <- -1]`, because 3 is less than or equal
  to 5.

### Control flow operations
FortikCpp provides three primitive control flow operations that enable branching and looping.
These operations work with recorded words (created using `[` and `]`) and numeric condition
values on the value stack. A numeric value is considered truthy when it is non-zero, and falsy
when it is zero.

- `IfElse` operation is written as `ifelse` and it pops three values from the stack: first an
  else word, then a then word, and finally a condition number. If the condition is truthy
  (non-zero), the then word is evaluated; otherwise the else word is evaluated. It turns a stack
  of the form `[ ... <- $cond <- $then <- $else ]` into the result of evaluating either `$then`
  or `$else`. For example:
  ```
  5 3 < [ 10 ] [ 20 ] ifelse
  ```
  Since `5 3 <` pushes `0` (falsy, because 5 is not less than 3), the else word `[ 20 ]` is
  evaluated, resulting in a stack of `[| <- 20]`.

- `ForLoop` operation is written as `for` and it pops a body word, an end number, and a start
  number from the stack. It then iterates from `start` to `end` (inclusive), evaluating the body
  word on each iteration. During each iteration, the current index is available as a variable
  named `i` on the word stack, meaning that referencing `i` inside the body pushes the current
  iteration counter onto the value stack. It turns a stack of the form
  `[ ... <- $start <- $end <- $body ]` into the result of evaluating `$body` for each value of
  `i` from `$start` to `$end`. For example:
  ```
  0 1 5 [ i + ] for
  ```
  This iterates `i` from 1 to 5, adding `i` to the accumulator on each iteration. The stack
  transitions as follows: `[| <- 0]` → `[| <- 1]` → `[| <- 3]` → `[| <- 6]` → `[| <- 10]` →
  `[| <- 15]`, resulting in a final stack of `[| <- 15]`.

- `WhileLoop` operation is written as `while` and it pops a body word and a condition word from
  the stack. It repeatedly evaluates the condition word, which must leave a number on the value
  stack. If that number is truthy (non-zero), the body word is evaluated. This cycle continues
  until the condition evaluates to zero (falsy), at which point the loop terminates. It turns a
  stack of the form `[ ... <- $cond <- $body ]` into the result of repeatedly evaluating `$body`
  while `$cond` remains truthy. For example:
  ```
  0 [ dup 5 < ] [ inc ] while
  ```
  Here, the condition `[ dup 5 < ]` checks whether the top of the stack is less than 5, and the
  body `[ inc ]` increments it. Starting from `0`, the loop increments until the value reaches
  `5`, at which point `5 5 <` evaluates to `0` (falsy) and the loop stops, resulting in a stack
  of `[| <- 5]`.

In addition, there are two standard words implemented using these primitive operations:

- `when` is defined as `[ [ ] ifelse ] :when` and it provides conditional execution without an
  else branch. It expects a condition number and a then word on the stack. When called, it
  pushes an empty word `[ ]` (which does nothing) as the else branch and then invokes `ifelse`.
  If the condition is truthy, the then word is evaluated; otherwise nothing happens. It turns a
  stack of the form `[ ... <- $cond <- $then ]` into the result of evaluating `$then` when
  `$cond` is truthy, or leaves the stack unchanged otherwise. For example:
  ```
  3 5 < [ 42 ] when
  ```
  Since `3 5 <` pushes `-1` (truthy, because 3 is less than 5), the then word `[ 42 ]` is
  evaluated, resulting in a stack of `[| <- 42]`.

- `times` is defined as `[ 1 rot-r for ] :times` and it provides a simple counted loop. It
  expects a count number and a body word on the stack. When called, it pushes `1`, rotates the
  stack right (so the order becomes `1`, `count`, `body`), and then invokes `for`. This causes
  the body to be evaluated `count` times, with the loop variable `i` ranging from `1` to
  `count`. It turns a stack of the form `[ ... <- $count <- $body ]` into the result of
  evaluating `$body` for each `i` from `1` to `$count`. For example:
  ```
  0 5 [ dup inc + ] times inc
  ```
  Starting from `0`, each iteration duplicates the top, increments the copy, and adds them
  together. After 5 iterations the value is `31`, and the final `inc` brings it to `32`,
  resulting in a stack of `[| <- 32]`.

### Printing operations
There is one primitive printing operation that outputs a number from the value stack to standard
output:

- `Print` operation is written as `.` and it pops a number from the top of a stack and prints
  it to standard output followed by a newline. It turns a stack of the form `[ ... <- $x ]` into
  `[ ... ]`. For example:
  ```
  42 .
  ```
  Evaluating this word would print `42` to standard output and result in an empty stack of
  `[| ]`.

  The `Print` operation is useful for debugging and for producing visible output during program
  execution. For example, the following program prints all numbers from 1 to 5:
  ```
  1 5 [ i . ] for
  ```
  This iterates `i` from 1 to 5, printing each value. The output would be:
  ```
  1
  2
  3
  4
  5
  ```

### Calling words
When a token in a program is not recognized as any of the primitive operations (such as `+`,
`dup`, `ifelse`, etc.) and does not start with `:` or `!`, the interpreter treats it as a
`CallWord` operation. This means that any sequence of printable characters that is not a
primitive token or a special-prefixed token is interpreted as a call to a named word on the word
stack.

- `CallWord` operation is not written with a fixed literal — instead, any token that does not
  match a primitive operation is treated as a call. When evaluated, it searches the word stack
  from top to bottom for the first entry whose key matches the token's interned name. If found,
  the associated word is evaluated. If no matching entry is found, an error is reported. For
  example:
  ```
  [ 36 67 42 + ] :compute
  compute
  ```
  Evaluating the word `compute` searches the word stack for the key `compute`, finds the
  recorded word `<36 67 42 +>`, and evaluates it. The stack transitions as follows:
  `[| <- 36 <- 67 <- 42]` → `[| <- 36 <- 109]`, resulting in a final stack of
  `[| <- 36 <- 109]`.

  Word calls can be nested, and a word can call itself recursively. For example, the following
  program defines a factorial word that calls itself:
  ```
  [ :n n 2 < [ 1 ] [ n n 1 - fact * ] ifelse ] :fact
  5 fact .
  ```
  Here, `fact` is defined recursively: if `n` is less than 2, it pushes `1`; otherwise it
  computes `n * (n-1)!` by calling `fact` again. Evaluating `5 fact .` would print `120` to
  standard output.

  When a word is called, a `word_stack_guard_t` is used to restore the word stack to its
  previous state after the call completes. This means that any new word bindings introduced
  during the execution of a word (e.g., via `:name`) are removed when the call returns, keeping
  the word stack scoped to the call.

  > **Warning:** Word scoping is **not lexical**. When a recorded word references another word
  > by name, that reference is resolved at the time of the actual call, not at the time the
  > word was recorded. This means that if a word `A` uses a word `B`, and `B` is later
  > redefined, calling `A` will use the new definition of `B`. This late-binding behavior
  > makes mutual recursion straightforward to set up — both words can reference each other
  > before either is fully defined — but it also demands greater discipline from the
  > programmer, as redefining a word may silently change the behavior of words that depend on
  > it.

### Other standard operations not covered in prev sections
In addition to the words covered in previous sections, there are three standard words that
operate on numbers and are implemented using primitive operations:

- `max` (maximum) is defined as
  `[ :max-rhs :max-lhs max-lhs max-rhs > [ max-lhs ] [ max-rhs ] ifelse ] :max` and it pops two
  numbers, compares them, and pushes the larger one. It turns a stack of the form
  `[ ... <- $x <- $y ]` into `[ ... <- ($x > $y ? $x : $y) ]`. For example:
  ```
  3 7 max
  ```
  Evaluating this word would result in a stack of `[| <- 7]`.

- `min` (minimum) is defined as
  `[ :min-rhs :min-lhs min-lhs min-rhs < [ min-lhs ] [ min-rhs ] ifelse ] :min` and it pops two
  numbers, compares them, and pushes the smaller one. It turns a stack of the form
  `[ ... <- $x <- $y ]` into `[ ... <- ($x < $y ? $x : $y) ]`. For example:
  ```
  3 7 min
  ```
  Evaluating this word would result in a stack of `[| <- 3]`.

- `abs` (absolute value) is defined as
  `[ :abs-op abs-op 0 > [ abs-op ] [ abs-op neg ] ifelse ] :abs` and it pops a number, checks
  whether it is greater than zero, and pushes the number itself if so, or its negated value
  otherwise. It turns a stack of the form `[ ... <- $x ]` into `[ ... <- |$x| ]`. For example:
  ```
  42 neg abs
  ```
  Evaluating this word would result in a stack of `[| <- 42]`.
