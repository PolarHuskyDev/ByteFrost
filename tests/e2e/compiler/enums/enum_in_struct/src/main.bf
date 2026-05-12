enum Role { ADMIN, USER, GUEST }
struct Account {
    name: string;
    role: Role;
}
main(): int {
    acc: Account = { name: "carol", role: Role.ADMIN };
    match(acc.role) {
        Role.ADMIN => { print("admin access"); }
        Role.USER => { print("user access"); }
        Role.GUEST => { print("guest access"); }
    }
    return 0;
}
