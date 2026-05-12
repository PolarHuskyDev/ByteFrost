enum Suit { HEARTS, DIAMONDS }
enum Rank { ACE, KING }
main(): int {
    s: Suit = Suit.HEARTS;
    r: Rank = Rank.ACE;
    match(s) {
        Suit.HEARTS => {
            match(r) {
                Rank.ACE => { print("ace of hearts"); }
                Rank.KING => { print("king of hearts"); }
            }
        }
        Suit.DIAMONDS => {
            print("diamonds");
        }
    }
    return 0;
}
