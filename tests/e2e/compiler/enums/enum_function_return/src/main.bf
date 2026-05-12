enum Status { OK, ERROR, PENDING }
get_status(code: int): Status {
    if (code == 0) { return Status.OK; }
    if (code == 1) { return Status.ERROR; }
    return Status.PENDING;
}
main(): int {
    s: Status = get_status(1);
    match(s) {
        Status.OK => { print("ok"); }
        Status.ERROR => { print("error"); }
        Status.PENDING => { print("pending"); }
    }
    return 0;
}
