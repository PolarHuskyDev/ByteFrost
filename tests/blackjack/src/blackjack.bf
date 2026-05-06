import Deck from deck;
import Card, CardRanks from cards;

export struct BlackJack {
    deck: Deck;

    card_value(c: Card): int {
        v: int = 0;
        match(c.rank) {
            CardRanks.TWO   => { v = 2;  }
            CardRanks.THREE => { v = 3;  }
            CardRanks.FOUR  => { v = 4;  }
            CardRanks.FIVE  => { v = 5;  }
            CardRanks.SIX   => { v = 6;  }
            CardRanks.SEVEN => { v = 7;  }
            CardRanks.EIGHT => { v = 8;  }
            CardRanks.NINE  => { v = 9;  }
            CardRanks.TEN | CardRanks.JACK | CardRanks.QUEEN | CardRanks.KING => { v = 10; }
            CardRanks.ACE   => { v = 11; }
            _ => { v = 0; }
        }
        return v;
    }

    hand_total(hand: array<Card>): int {
        sum: int = 0;
        aces: int = 0;
        i: int = 0;
        while (i < hand.length()) {
            c: Card = hand[i];
            v: int = this.card_value(c);
            sum = sum + v;
            if (c.rank == CardRanks.ACE) {
                aces = aces + 1;
            }
            i = i + 1;
        }
        while (aces > 0 && sum > 21) {
            sum = sum - 10;
            aces = aces - 1;
        }
        return sum;
    }

    play_round(player_name: string): void {
        player: array<Card>;
        dealer: array<Card>;
        print("--- New Round ---");
        print("{player_name}'s cards:");
        c1: Card = this.deck.deal();
        player.push(c1);
        print("  {c1.rank} of {c1.suit}");
        c2: Card = this.deck.deal();
        player.push(c2);
        print("  {c2.rank} of {c2.suit}");
        print("Dealer shows: [hidden] +");
        d1: Card = this.deck.deal();
        dealer.push(d1);
        dc2: Card = this.deck.deal();
        dealer.push(dc2);
        print("  {dc2.rank} of {dc2.suit}");
        player_bust: bool = false;
        player_done: bool = false;
        while (!player_done) {
            pt_cur: int = this.hand_total(player);
            print("{player_name} total: {pt_cur}");
            if (this.hand_total(player) >= 21) {
                player_done = true;
            } else {
                action: string = input("Hit or Stand? (h/s): ");
                if (action == "h") {
                    print("You drew:");
                    cx: Card = this.deck.deal();
                    player.push(cx);
                    print("  {cx.rank} of {cx.suit}");
                    if (this.hand_total(player) > 21) {
                        player_bust = true;
                        player_done = true;
                    }
                } else {
                    player_done = true;
                }
            }
        }
        if (player_bust) {
            print("{player_name} busts! Dealer wins.");
            return;
        }
        if (this.hand_total(player) == 21) {
            print("{player_name} has 21!");
        }
        print("--- Dealer's Turn ---");
        dt_cur: int = this.hand_total(dealer);
        print("Dealer total: {dt_cur}");
        while (this.hand_total(dealer) < 17) {
            print("Dealer hits:");
            dx: Card = this.deck.deal();
            dealer.push(dx);
            print("  {dx.rank} of {dx.suit}");
            dt2: int = this.hand_total(dealer);
            print("Dealer total: {dt2}");
        }
        pt: int = this.hand_total(player);
        dt: int = this.hand_total(dealer);
        print("{player_name} final: {pt}");
        print("Dealer final: {dt}");
        if (dt > 21) {
            print("Dealer busts! You win!");
        } elseif (pt > dt) {
            print("You win!");
        } elseif (pt == dt) {
            print("Push (tie)!");
        } else {
            print("Dealer wins!");
        }
    }

    run(player_name: string): void {
        print("=== Blackjack ===");
        playing: bool = true;
        while (playing) {
            this.play_round(player_name);
            again: string = input("Play again? (y/n): ");
            if (again == "n") {
                playing = false;
            }
        }
        print("Thanks for playing!");
    }
}
