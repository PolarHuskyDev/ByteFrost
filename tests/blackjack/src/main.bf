// main.bf — Entry point for the Blackjack game.
// Imports BlackJack and starts the game loop.

import BlackJack from blackjack;

main(): int {
    srand(time(0));
    name: string = input("Enter your name: ");
    game: BlackJack = BlackJack();
    game.run(name);
    return 0;
}
