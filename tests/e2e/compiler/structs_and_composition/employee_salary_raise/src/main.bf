struct Employee {
    name: string;
    salary: float;
    constructor(n: string, s: float) {
        this.name = n;
        this.salary = s;
    }
    raise(pct: float): void {
        this.salary = this.salary * (1.0 + pct / 100.0);
    }
}
main(): int {
    e: Employee = Employee("Alice", 50000.0);
    e.raise(10.0);
    print(e.salary);
    return 0;
}
