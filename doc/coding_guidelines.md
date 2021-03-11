# Formatting
## Naming
- Macros: `CODEPAD_ALL_CAPS_WITH_UNDERSCORES`. Prefix macros with `CODEPAD_` or `CP_`.
- Template arguments: `PascalCase`.
- Names of classes, structs, enum classes, functions, methods, variables, etc.: `snake_case`.
	- For all entities that should not be visible to the outer scope, their names should start with an underscore. For example:
		- `private: int _private_field;`
		- `protected: void _protected_function(int foo);`
		- `private: struct _internal_struct;`
		- `int foo() { static int _function_local_static; }`
		- Use the `_details` namespace for functions visible only to the current namespace: `namespace codepad::some_namespace::_details;`
- For names of classes, structs, enum classes, functions, methods, and public fields, prefer whole words and descriptive names even if they may be longer.
	- Sometimes using nested types can help rid of duplicate prefixes. For example, `class car` and `struct car::wheel` instead of `class car` and `struct car_wheel`.

## Indentation, New Lines, Spacing, and Wrapping
There's currently a `.clang-format` file in the project root, but it is not used: in codepad closing brackets are put on a new line, which is not yet supported by `clang-format`. More info at https://reviews.llvm.org/D33029. For now, use the `.clang-format` as a reference, but also try to follow the style of existing code.
- Indent using one tab.
- Wrap the code before 120 characters.
- Put brackets on the same line following one space.

# Language and Conventions
- Don't use multiple or virtual inheritance unless absolutely necessary.
- Use `enum class` instead of `enum`.
- Use `using` instead of `typedef`.
- Use `std::variant` instead of `union`.
- Don't use exceptions unless absolutely necessary.
- Don't use macros unless absolutely necessary in headers. It's ok to define and use macros in source (`cpp`) files.
- Don't use global variables unless absolutely necessary.
- Don't use `auto` for return types unless necessary.
- Use prefix increment/etc. operators instead of postfix increment/etc. operators whenever possible.
- Put all code under the `codepad` namespace if possible.
- Use UTF-8 for all internal operations. Use `std::u8string` and `std::u8string_view` instead of `std::string` and `std::string_view`. Use `std::u8string_view` instead of `const std::u8string&` unless a C-style string is expected.
- Use forward slashes (`/`) in include paths.

## Regarding `int`
Codepad uses unsigned integers (especially `std::size_t`) extensively. Try follow these rules when determining the type of a new integral variable:

- Firstly, if you're interfacing with a library, use whichever type the library uses.
- If you need an integral variable to store a count/index, use `std::size_t`.
- If you need an integral variable to store a count/index, but also need a special value to represent some edge case, you have the following options:
	- Use `std::optional<std::size_t>`.
	- Use `std::numeric_limits<std::size_t>::max()` as the special value.
	- If it makes sense and is easy to implement, you can try offseting all values by 1, and use 0 as the special value.
- Otherwise, use `int`.

Unlike signed integers, overflow is well-defined for unsigned integers. This means that addition and subtraction is safe for unsigned integers as long as you end up with a non-negative number, even if the intermediate values may be negative.

One of the potential risks is that we may miss out on some optimizations that the compiler would be able to make thanks to the undefined overflow behavior of signed integers. However, these are rare and can be easily solved on a case-by-case and on-demand basis.

# Comments
- Document all entities (classes, structs, function, methods, fields, etc.) in the code using [doxygen syntax](https://www.doxygen.nl/manual/docblocks.html).
	- Use `///` to start documentation, and `///<` to start inline documentation. Start special commands with `\`.
	- Document parameters/template parameters/return values if necessary. For any specific function/class, either don't document any (template) parameters parameters, or document all (template) parameters.
- Make comments in your code using `//`, and comment out code using `/*...*/`. This makes it easier to find commented code.
- Use notions such as `// TODO`, `// FIXME`, and `// HACK` to mark where more work needs to be done. You can also do this in the documentation using `/// \todo`.
- When having spent hours to fix one bug using a few lines, be sure to document why the fix is there for future reference.
