struct BankAccount {
    balance: float;
    constructor(initial: float) {
        this.balance = initial;
    }
    deposit(amount: float): void {
        this.balance = this.balance + amount;
    }
    withdraw(amount: float): void {
        if (amount <= this.balance) {
            this.balance = this.balance - amount;
        }
    }
    get_balance(): float {
        return this.balance;
    }
}
main(): int {
    acc: BankAccount = BankAccount(100.0);
    acc.deposit(50.0);
    acc.withdraw(30.0);
    print(acc.get_balance());
    return 0;
}
