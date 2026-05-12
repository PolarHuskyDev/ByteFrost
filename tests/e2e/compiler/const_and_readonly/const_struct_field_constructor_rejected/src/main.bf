struct Widget {
    const ID: int;
    label: string;

    constructor(id: int) {
        this.ID = id;
    }
}

main(): int {
    w: Widget = Widget(200);
    return 0;
}
