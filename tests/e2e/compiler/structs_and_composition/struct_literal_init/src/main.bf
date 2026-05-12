struct Person {
    name: string;
    age: int;
}
main(): int {
    p: Person = { name: "Alice", age: 30 };
    print(p.name);
    print(p.age);
    return 0;
}
