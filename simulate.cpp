#include <iostream>
#include "shiritorigame.h"

int main() {
    ShiritoriGame game;
    std::string dict = "/home/hiyodori/Words/Dictionaries/Last_Letter/last_letter.txt";
    std::string patterns = "/home/hiyodori/Words/Dictionaries/Last_Letter/rare_prefix.txt";

    std::cout << "Loading database: " << dict << "\n";
    if (!game.load_database(dict, patterns)) {
        std::cerr << "Failed to load database\n";
        return 1;
    }
    std::cout << "Database loaded.\n";

    // Start with an AI random start word
    std::string start = game.getRandomStartWord();
    std::cout << "AI start word: " << start << "\n";

    // Simulate AI putting that word in chain
    game.processPlayerWord(start); // reuse to add to chain (some implementations expect this)

    // Now simulate a few turns
    for (int turn = 0; turn < 6; ++turn) {
        std::string prefix = game.getCurrentPrefix();
        std::cout << "Turn " << turn+1 << ", current prefix: '" << prefix << "'\n";

        auto tops = game.getTopAIMoves(prefix, 6);
        std::cout << "Top AI moves (word -> creates_prefix_solutions):\n";
        for (size_t i=0;i<tops.size();++i) {
            std::cout << "  " << (i+1) << ") " << tops[i].word << " -> " << tops[i].creates_prefix_solutions << "\n";
        }

        // If there's a top move, simulate player choosing the first one
        if (!tops.empty()) {
            std::string playerChoice = tops[0].word;
            bool wasTop = game.wasTopSolve(playerChoice);
            std::cout << "Player submits: " << playerChoice << " (wasTopSolve? " << (wasTop ? "yes" : "no") << ")\n";
            game.processPlayerWord(playerChoice);
        } else {
            std::cout << "No top moves available; player picks random fallback.\n";
            std::string ai = game.getAIMove();
            if (ai.empty()) { std::cout << "AI cannot move; game over.\n"; break; }
            std::cout << "AI move: " << ai << "\n";
            game.processPlayerWord(ai);
        }

        // Let AI choose move
        std::string aiMove = game.getAIMove();
        if (aiMove.empty()) { std::cout << "AI cannot move; game ends.\n"; break; }
        std::cout << "AI replies with: " << aiMove << "\n";
        game.processPlayerWord(aiMove);
    }

    // Print final chain
    const auto& chain = game.getWordChain();
    std::cout << "Final word chain (" << chain.size() << " words):\n";
    for (size_t i=0;i<chain.size();++i) std::cout << "  " << (i+1) << ") " << chain[i] << "\n";

    return 0;
}
