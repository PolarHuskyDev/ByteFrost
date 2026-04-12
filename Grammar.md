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
      "null"
    | identifier                        (* binding pattern *)
    | expression { "|" expression } ;

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
    | identifier "{" field_init_list? "}"   (* named struct init *)
    | identifier
    | "(" expression ")" ;

field_init_list =
    identifier ":" expression { "," identifier ":" expression } ;

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
    | boolean_literal
    | "null"
    | "[" [ expression { "," expression } ] "]" ;

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
