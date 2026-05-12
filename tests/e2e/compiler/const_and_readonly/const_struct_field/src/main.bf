struct Config {
    const VERSION: int;
    name: string;
}

main(): int {
    c: Config = { VERSION: 42, name: "release" };
    print(c.VERSION);
    print(c.name);
    return 0;
}
