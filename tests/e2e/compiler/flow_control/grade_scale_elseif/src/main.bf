grade(score: int): void {
    if (score >= 90) {
        print("A");
    } elseif (score >= 80) {
        print("B");
    } elseif (score >= 70) {
        print("C");
    } elseif (score >= 60) {
        print("D");
    } else {
        print("F");
    }
}
main(): int {
    grade(95);
    grade(83);
    grade(71);
    grade(60);
    grade(45);
    return 0;
}
