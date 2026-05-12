struct Config {
    readonly max_size: int;
    constructor(size: int) {
        this.max_size = size;
    }
}
main(): int {
    cfg: Config = Config(256);
    print(cfg.max_size);
    return 0;
}
