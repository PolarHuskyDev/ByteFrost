import CardRanks, CardSuits, Card from cards;

export struct Deck {
    cards: array<Card>;
    pos:   int;

    constructor() {
        this.pos = 0;
        suits: array<CardSuits> = [CardSuits.HEARTS, CardSuits.DIAMONDS, CardSuits.CLUBS, CardSuits.SPADES];
        ranks: array<CardRanks> = [CardRanks.TWO, CardRanks.THREE, CardRanks.FOUR, CardRanks.FIVE,
                                   CardRanks.SIX, CardRanks.SEVEN, CardRanks.EIGHT, CardRanks.NINE,
                                   CardRanks.TEN, CardRanks.JACK, CardRanks.QUEEN, CardRanks.KING, CardRanks.ACE];
        for (i: int = 0; i < suits.length(); i++) {
            for (j: int = 0; j < ranks.length(); j++) {
                c: Card = { rank: ranks[j], suit: suits[i] };
                this.cards.push(c);
            }
        }
        this.shuffle();
    }

    shuffle(): void {
        n: int = this.cards.length();
        i: int = n - 1;
        while (i > 0) {
            j: int = rand() % (i + 1);
            tmp: Card = this.cards[i];
            this.cards[i] = this.cards[j];
            this.cards[j] = tmp;
            i = i - 1;
        }
        this.pos = 0;
    }

    deal(): Card {
        if (this.pos >= this.cards.length()) {
            this.shuffle();
        }
        c: Card = this.cards[this.pos];
        this.pos = this.pos + 1;
        return c;
    }
}
