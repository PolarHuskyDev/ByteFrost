main(): int {
    day: string = "FRIDAY";
    match(day) {
        "MONDAY" | "TUESDAY" | "WEDNESDAY" | "THURSDAY" | "FRIDAY" => {
            print("weekday");
        }
        "SATURDAY" | "SUNDAY" => {
            print("weekend");
        }
        _ => { print("unknown"); }
    }
    return 0;
}
