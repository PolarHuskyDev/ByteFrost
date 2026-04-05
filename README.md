# ByteFrost programming language

> The bible for this project is [LLVM tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html)

ByteFrost is a systems language aiming for ease of use.

## Overall goal 

A simple hello world example would be

```bf
main(): int {
	print("Hello, World!");

	return 0;
}
```

### Primitive data types:

- int integer numbers
- float floating point numbers
- bool Boolean values (true, false)
- char single characters (scalar UTF-32)


### Standard library types

- complex (real + imaginary), basically a tuple of floats

Then a list of compound data types:

- string: group of chars
- array: group of any other type (int, bools, floats, objects or other arrays maybe?)

Note: Let me know where some datatypes do not belong here or are unnecesarry to be defined in this manner and can be created as some sort of object/class

Next, I would like to define a set of control flow constructs. These include if, else, elseif, for, while, pattern-matching (switch-case like?), break, continue:

- if/else example
```bf
main(): int {
	x: int = 3;

	if (x % 2 == 0) {
		print("Even");
	} else {
		print("Odd");
	}

	return 0;
}
```

For loop example
```bf
fib(n: int): int {
	if (n <= 1) {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}

main(): int {
	// Full for loop
	for (i: int = 0 ; i < 10 ; i++) {
		print(fib(i));
	}

	// For loop with iterator in range
	for (i: int in [0..9]) {
		print(fib(i));
	}

	return 0;
}
```

While loop example
```bf
main(): int {
	x: int = 0;
	while (x <= 10) {
		print(x);
	}

	return 0;
}
```

pattern matching example

```bf
main(): int {
	state: string = "HIGH"; // HIGH, LOW, MIDDLE, INTERMEDIATE

	match(state) {
		"HIGH" => {
			print("High state matched");
		}
		"LOW" => {
			print("Low state matched");
		}
		"MIDDLE" | "INTERMEDIATE" => {
			print("Middle or intermediate state matched");
		}
		_ => {
			print("None matched");
		}
	}

	return 0;
}
```

break and continue example
```bf
main(): int {
	x: int = 0;
	while (x <= 10) {
		if (x == 2) {
			continue; // skip 2 but move forward to 3
		}

		if (x == 7) {
			break; // stop and exit loop
		}

		print(x);
	}

	return 0;
}
```

Finally I want to provide the possibility to developers to extend the language defining custom types:

```bf
struct Person {
	name: string;
	age: int;

	salute(): void {
		print("Hi I am {this.name}, and I am {this.age} years old");
	}
}

main(): int {
	p: Person = {
		name: "Peter",
		age: 21
	};

	p.salute();
	return 0;
}
```

Finally I would love to provide async capability but unlike nodejs it is supposed to be threaded. Although this is for the very future and we will need to do a lot of research to reach this point.


```bf
async longTask(): int {
	sleep(1);
	return 1;
}

async main(): int {
	result: int = await longTask();

	print(result)
	return 0;
}

```

Right now I would like an overall evaluation from you in terms of clarity, readability and get an overall score of the language. I would also like to get some suggestions on what can be changed, added or improved

## Operators
### Arithmetic operators (The core)
|Operator|Usage|Example|
|--------|-----|-------|
|+|Addition|`x: int = 1 + 2;`|
|-|Subtraction|`x: int = 5 - 3;`|
|*|Multiplication|`x: int = 2 * 4;`|
|/|Division|`x: int = 10 / 2;`|
|%|Modulo (remainder)|`x: int = 10 % 3;`|

### Comparison operators
|Operator|Usage|Example|
|--------|-----|-------|
|==|Equal to|`if (x == 10) {}`|
|!=|Not equal to|`if (x != 0) {}`|
|<|Less than|`if (x < 5) {}`|
|>|Greater than|`if (x > 5) {}`|
|<=|Less than or equal|`if (x <= 10) {}`|
|>=|Greater than or equal|`if (x >= 1) {}`|

### Logical operators
|Operator|Usage|Example|
|--------|-----|-------|
|&&|Logical AND|`if (a > 0 && b > 0) {}`|
|\|\||Logical OR|`if (a > 0 \|\| b > 0) {}`|
|!|Logical NOT|`if (!done) {}`|
|^^|Logical XOR|`if (a > 0 ^^ b > 0) {}`|

### Bitwise operators
|Operator|Usage|Example|
|--------|-----|-------|
|&|Bitwise AND|`x := a & 0xFF;`|
|\||Bitwise OR|`x := a \| 0x0F;`|
|^|Bitwise XOR|`x := a ^ b;`|
|~|Bitwise NOT|`x := ~a;`|
|<<|Left shift|`x := a << 2;`|
|>>|Right shift|`x := a >> 1;`|

### Assignment operators
|Operator|Usage|Example|
|--------|-----|-------|
|=|Assignment|`x = 10;`|
|:=|Declare & assign (inferred type)|`x := 10;`|
|+=|Add and assign|`x += 5;`|
|-=|Subtract and assign|`x -= 3;`|
|*=|Multiply and assign|`x *= 2;`|
|/=|Divide and assign|`x /= 4;`|
|%=|Modulo and assign|`x %= 3;`|

### Increment / Decrement operators
|Operator|Usage|Example|
|--------|-----|-------|
|++|Post-increment|`x++;`|
|--|Post-decrement|`x--;`|

### Unary operators
|Operator|Usage|Example|
|--------|-----|-------|
|-|Negation|`x := -y;`|
|!|Logical NOT|`x := !flag;`|
|~|Bitwise NOT|`x := ~mask;`|

### Member access / Other
|Operator|Usage|Example|
|--------|-----|-------|
|.|Member access|`p.name`|
|()|Function call|`foo(1, 2)`|
|:|Type annotation|`x: int = 0;`|
|[i]|Array element access|`a[0]`|
|["k"]|Map element access|`m["key"]`|


## EBNF (Extended Backus–Naur Form) - formal grammar description
```ebnf
(* ========================= *)
(* Program Structure *)
(* ========================= *)

program = { function_declaration | struct_declaration } ;

function_declaration =
    identifier "(" parameter_list? ")" ":" type block ;

struct_declaration =
    "struct" identifier "{"
        { struct_member }
    "}" ;

struct_member =
      identifier ":" type ";"
    | function_declaration ;

parameter_list = parameter { "," parameter } ;
parameter      = identifier ":" type ;

(* ========================= *)
(* Types *)
(* ========================= *)

type =
      "int"
    | "float"
    | "bool"
    | "char"
    | "string"
    | "void"
    | identifier
    | type "?"
    | "array" "<" type ">"
    | "slice" "<" type ">"
    | "map" "<" type "," type ">" ;

(* ========================= *)
(* Blocks & Statements *)
(* ========================= *)

block = "{" { statement } "}" ;

statement =
      variable_declaration
    | assignment_statement
    | if_statement
    | while_statement
    | for_statement
    | match_statement
    | return_statement
    | break_statement
    | continue_statement
    | expression_statement ;

(* ========================= *)
(* Variable Declaration *)
(* ========================= *)

variable_declaration =
      identifier ":" type [ "=" expression ] ";"
    | identifier ":=" expression ";" ;

assignment_statement =
    lvalue assignment_operator expression ";" ;

lvalue =
      identifier
    | lvalue "." identifier
    | lvalue "[" expression "]" ;

assignment_operator =
      "="
    | "+="
    | "-="
    | "*="
    | "/="
    | "%=" ;

(* ========================= *)
(* Control Flow *)
(* ========================= *)

if_statement =
    "if" "(" expression ")" block
    { "elseif" "(" expression ")" block }
    [ "else" block ] ;

while_statement =
    "while" "(" expression ")" block ;

for_statement =
      "for" "("
          (variable_declaration | assignment_statement)? ";"
          expression? ";"
          expression?
      ")" block
    | "for" "(" identifier ":" type "in" range ")" block ;

range = "[" expression ".." expression "]" ;

(* ========================= *)
(* Match *)
(* ========================= *)

match_statement =
    "match" "(" expression ")" "{"
        { match_case }
        [ default_case ]
    "}" ;

match_case =
    pattern "=>" block ;

pattern =
    expression { "|" expression } ;

default_case =
    "_" "=>" block ;

(* ========================= *)
(* Flow Control *)
(* ========================= *)

return_statement =
    "return" expression? ";" ;

break_statement = "break" ";" ;
continue_statement = "continue" ";" ;

expression_statement =
    expression ";" ;

(* ========================= *)
(* Expressions *)
(* ========================= *)

expression = assignment_expression ;

assignment_expression =
    logical_or_expression
    [ assignment_operator assignment_expression ] ;

logical_or_expression =
    logical_xor_expression
    { "||" logical_xor_expression } ;

logical_xor_expression =
    logical_and_expression
    { "^^" logical_and_expression } ;

logical_and_expression =
    bitwise_or_expression
    { "&&" bitwise_or_expression } ;

bitwise_or_expression =
    bitwise_xor_expression
    { "|" bitwise_xor_expression } ;

bitwise_xor_expression =
    bitwise_and_expression
    { "^" bitwise_and_expression } ;

bitwise_and_expression =
    equality_expression
    { "&" equality_expression } ;

equality_expression =
    relational_expression
    { ( "==" | "!=" ) relational_expression } ;

relational_expression =
    shift_expression
    { ( "<" | ">" | "<=" | ">=" ) shift_expression } ;

shift_expression =
    additive_expression
    { ( "<<" | ">>" ) additive_expression } ;

additive_expression =
    multiplicative_expression
    { ( "+" | "-" ) multiplicative_expression } ;

multiplicative_expression =
    unary_expression
    { ( "*" | "/" | "%" ) unary_expression } ;

unary_expression =
      ( "-" | "!" | "~" ) unary_expression
    | postfix_expression ;

postfix_expression =
    primary_expression
    {
        "(" argument_list? ")"
      | "[" expression "]"
      | "." identifier
      | "++"
      | "--"
    } ;

primary_expression =
      literal
    | "this"
    | identifier
    | "(" expression ")" ;

argument_list =
    expression { "," expression } ;

(* ========================= *)
(* Literals *)
(* ========================= *)

literal =
      integer_literal
    | float_literal
    | string_literal
    | interpolated_string
    | char_literal
    | boolean_literal ;

boolean_literal = "true" | "false" ;

(* ========================= *)
(* Lexical Elements *)
(* ========================= *)

identifier = ( letter | "_" ) { letter | digit | "_" } ;

integer_literal =
      digit { digit }
    | "0x" hex_digit { hex_digit }
    | "0b" bin_digit { bin_digit } ;

float_literal =
      digit { digit } "." digit { digit }
    | "." digit { digit }
    | digit { digit } "." ;

string_literal  = '"' { character } '"' ;

interpolated_string =
    '"' { character | "{" expression "}" } '"' ;

char_literal    = "'" character "'" ;

(* ========================= *)
(* Character Definitions *)
(* ========================= *)

letter    = "A".."Z" | "a".."z" ;
digit     = "0".."9" ;
hex_digit = "0".."9" | "A".."F" | "a".."f" ;
bin_digit = "0" | "1" ;
character = ? any valid UTF-8 character ? ;

```
