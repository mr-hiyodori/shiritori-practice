#ifndef SHIRITORIGAME_H
#define SHIRITORIGAME_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <bitset>

// Constants
const int MAX_PREFIX_LEN = 4;
const int STARTING_HEARTS = 2;
const int TOP_MOVES_TO_SHOW = 4;
const int POINTS_FOR_HEART = 5;

class ShiritoriGame {
private:
    std::vector<std::string> dict;
    std::vector<std::string> rev_dict;
    std::vector<std::string> patterns;
    std::unordered_map<std::string, int> prefix_count_cache;
    
    std::unordered_set<std::string> used_words;
    std::unordered_set<std::string> exhausted_prefixes;
    std::unordered_set<std::string> solved_suffixes;
    std::vector<std::string> word_chain;
    
    std::mt19937 rng;
    int turn_count;
    int turns_since_heart_loss;
    int player_hearts;
    int player_points;
    std::bitset<26> letters_used;
    
    std::string current_prefix;
    std::vector<std::string> last_top_moves;
    
    bool has_unused_words(const std::string& prefix) const;
    std::string find_valid_prefix(const std::string& word, int max_difficulty) const;

public:
    ShiritoriGame();
    
    bool load_database(const std::string& dict_file, const std::string& patterns_file);
    void reset_game();
    
    bool is_valid_word(const std::string& word);
    bool is_used(const std::string& word);
    
    std::string getCurrentPrefix() const;
    int getCurrentDifficulty() const;
    std::vector<std::string> getTopMoves(const std::string& prefix) const;
    std::string getRandomStartWord();
    void processPlayerWord(const std::string& word);
    std::string getAIMove();
    bool wasTopSolve(const std::string& word) const;
    
    int getPlayerHearts() const { return player_hearts; }
    int getPlayerPoints() const { return player_points; }
    const std::vector<std::string>& getWordChain() const { return word_chain; }
};

#endif // SHIRITORIGAME_H
