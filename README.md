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
TODO

### Comparison operations
TODO

### Control flow operations
TODO

### Printing operations
TODO

### Calling words
TODO
