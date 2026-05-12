struct Widget {
    const ID: int;
    label: string;

    reset(): void {
        this.ID = 0;
    }
}

main(): int {
    w: Widget = { ID: 100, label: "btn" };
    w.reset();
    return 0;
}
