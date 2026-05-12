enum Priority { LOW, MED, HIGH }
main(): int {
    p: Priority = Priority.HIGH;
    if (p == Priority.HIGH) {
        print("urgent");
    } elseif (p == Priority.MED) {
        print("normal");
    } else {
        print("low");
    }
    return 0;
}
