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
- array<T>: group of any other type
- map<K, V>: Key value pair mapping

Note: Let me know where some data types do not belong here or are unnecessary to be defined in this manner and can be created as some sort of object/class

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
	for (i: int = 0 ; i < 10 ; i++) {
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
		x++;
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

Containers
```bf
main(): int {
	list: array<int> = [0, 1, 2, 3, 4, 5];
	squared: array<int>;
	for (i: int = 0 ; i < list.length() ; i++ ) {
		squared.push( list[i] * list[i] );
	}

	print(list); // [0, 1, 2, 3, 4, 5];
	print(squared); // [0, 1, 4, 9, 16, 25];

	return 0;
}
```

```bf
main(): int {
	httpCodes: map<int, string>;
	httpCodes[200] = "Ok";
	httpCodes[201] = "Created";
	httpCodes[404] = "Not Found";

	print(httpCodes);
	// {
	//   200: "Ok",
	//   201: "Created",
	//   404: "Not Found"
	// }

	return 0;
}
```

### Struct Composition

Structs can contain fields of other struct types, enabling composition:

```bf
struct Point {
	x: int;
	y: int;
}

struct Circle {
	center: Point;
	radius: float;
}

main(): int {
	c: Circle = {
		center: { x: 10, y: 20 },
		radius: 5.5
	};

	print("Circle center: ({c.center.x}, {c.center.y}), radius: {c.radius}");
	// Circle center: (10, 20), radius: 5.5

	return 0;
}
```

Nested structs are embedded inline (value semantics) — no pointers or heap allocation. Chained member access (`c.center.x`) is fully supported, including in interpolated strings.

## Memory Model: Reference Counting

ByteFrost uses **reference counting** to manage heap-allocated memory for container types (`array<T>` and `map<K,V>`).

### How it works

Each array and map carries a hidden refcount pointer alongside its data:

| Type | Layout |
|------|--------|
| `array<T>` | `{ ptr data, i64 length, i64 capacity, ptr refcount }` |
| `map<K,V>` | `{ ptr keys, ptr values, i64 length, i64 capacity, ptr refcount }` |

- **Creation**: When an array or map is created, a refcount is `malloc`'d, initialized to 1, and stored in the struct.
- **Scope exit**: When a variable goes out of scope (function return), the refcount is decremented. If it reaches zero, all heap buffers (data, keys, values) and the refcount itself are freed.
- **Structs**: User-defined structs are stack-allocated value types — no refcounting needed. Composition (struct-in-struct) embeds fields inline with no heap allocation.

### Why reference counting?

| Criterion | RAII (deep copy) | Reference Counting | Tracing GC |
|-----------|------------------|--------------------|------------|
| Sharing | ✗ always copies | ✓ cheap aliasing | ✓ cheap aliasing |
| Deterministic free | ✓ | ✓ | ✗ |
| Runtime overhead | copy cost | increment/decrement | pause times |
| Implementation complexity | low | moderate | high |

Reference counting gives deterministic deallocation with cheap sharing — a good balance for a systems-oriented language.

### Cycle safety

Reference cycles (A → B → A) cannot occur in ByteFrost because:
- Structs are **value types** embedded inline — they cannot form pointer cycles.
- Arrays and maps hold primitive values or struct copies, not references to other containers.
- **Cyclic struct dependencies are rejected at compile time** — both direct (`Node` contains `Node`) and indirect (`A` contains `B` which contains `A`). The compiler performs a full dependency graph analysis before registering any struct types.

This is a structural guarantee: no cycle collector is needed.

### Cyclic struct limitation

ByteFrost currently does not support recursive data structures like linked lists or trees directly. The compiler rejects both **direct** and **indirect** cycles:

```bf
// Direct: struct references itself
struct Node {
    value: int;
    next: Node;  // ERROR
}

// Indirect: A → B → A
struct A { b: B; }
struct B { a: A; }  // ERROR
```

The compiler traces the full cycle path in the error message:
```
Codegen error: Cyclic struct dependency detected: Node -> Node.
Structs are value types and cannot form cycles (infinite size).
Future: use Box<T> for heap-allocated indirection.
```
```
Codegen error: Cyclic struct dependency detected: A -> B -> A. ...
```

**Why?** Structs are value types stored inline on the stack. A `Node` containing another `Node` (directly or through a chain of intermediate structs) would require infinite storage. Every language shares this fundamental constraint — C uses pointers (`struct Node*`), Rust uses `Box<Node>`, Java/Python use reference semantics.

**Future path — `Box<T>`:** A heap-allocated, reference-counted pointer type would enable recursive structures:
```bf
struct Node {
    value: int;
    next: Box<Node>;  // 8-byte pointer to heap-allocated Node
}
```
`Box<T>` would integrate with the existing refcount infrastructure — `malloc` on creation, decrement/free on scope exit. Note: `Box` fields would allow cycles, requiring either weak references (`Weak<T>`) or user discipline.


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

