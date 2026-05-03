export enum CardRanks { TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, ACE }
export enum CardSuits { HEARTS, DIAMONDS, CLUBS, SPADES }

export struct Card {
    rank: CardRanks;
    suit: CardSuits;
}
