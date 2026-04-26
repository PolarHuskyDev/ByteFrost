// null_safety — tests nullable struct semantics.
//
// Design: struct types are heap-allocated and nullable (pointer semantics).
//   - `r: Rectangle;`           defaults to null
//   - `r: Rectangle = null;`    explicit null
//   - `if (r) { ... }`          truthy when r is not null
//   - `if (!r) { ... }`         truthy when r is null
//   - Field access on non-null struct works normally.
//
// Primitives (int, float, string, bool) are never null; they have neutral defaults.
// Arrays and maps default to empty.

struct Rectangle {
    width: float;
    height: float;

    constructor(w: float, h: float) {
        this.width = w;
        this.height = h;
    }

    area(): float {
        return this.width * this.height;
    }
}

// ============================================================
// [BASELINE] Struct defaults to null with no initializer.
// ============================================================
testDefaultNull(): void {
    r: Rectangle;
    if (r) {
        print("FAIL: r should be null");
    } else {
        print("PASS: r is null by default");
    }
}

// ============================================================
// [2] Explicit null assignment.
// ============================================================
testExplicitNull(): void {
    r: Rectangle = null;
    if (!r) {
        print("PASS: r is null via explicit assignment");
    } else {
        print("FAIL: r should be null");
    }
}

// ============================================================
// [3] Null check after initializing via constructor.
// ============================================================
testInitialized(): void {
    r: Rectangle = Rectangle(3.0, 4.0);
    if (r) {
        a: float = r.area();
        print("PASS: r is not null, area = {a}");
    } else {
        print("FAIL: r should not be null after init");
    }
}

// ============================================================
// [4] Reassign to null after initialization.
// ============================================================
testReassignNull(): void {
    r: Rectangle = Rectangle(1.0, 2.0);
    r = null;
    if (!r) {
        print("PASS: r is null after reassignment");
    } else {
        print("FAIL: r should be null after reassignment");
    }
}

// ============================================================
// [5] Struct init syntax with field list, then null check.
// ============================================================
testStructInit(): void {
    r: Rectangle = {
        width: 5.0,
        height: 6.0
    };
    if (r) {
        a: float = r.area();
        print("PASS: struct init, area = {a}");
    } else {
        print("FAIL: struct init should not be null");
    }
}

main(): int {
    testDefaultNull();
    testExplicitNull();
    testInitialized();
    testReassignNull();
    testStructInit();
    return 0;
}
