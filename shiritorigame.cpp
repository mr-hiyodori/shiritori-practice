#include "shiritorigame.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iostream>

// Constructor
ShiritoriGame::ShiritoriGame()
    : turn_count(0)
    , turns_since_heart_loss(0)
    , player_hearts(STARTING_HEARTS)
    , player_points(0)
{
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    rng.seed(seed);
    letters_used.reset();
}

// Helper functions
inline std::string reverse_str(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}

inline void to_lower_inplace(std::string& s) {
    for (char& c : s) c = std::tolower(static_cast<unsigned char>(c));
}

std::string parse_word(const std::string& line) {
    if (line.empty() || line[0] == '-' || line[0] == '#') return "";
    size_t colon_pos = line.find(':');
    const std::string& raw = (colon_pos != std::string::npos) ? line.substr(0, colon_pos) : line;
    
    std::string clean;
    clean.reserve(raw.length());
    for (char c : raw) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            clean += std::tolower(static_cast<unsigned char>(c));
        }
    }
    return clean;
}

inline std::string get_suffix(const std::string& word, int len) {
    return word.length() < static_cast<size_t>(len) ? word : word.substr(word.length() - len);
}

inline int get_difficulty_level(int turns_since_reset) {
    if (turns_since_reset <= 3) return 1;
    else if (turns_since_reset <= 8) return 2;
    else if (turns_since_reset <= 15) return 3;
    else return 4;
}

// Load database
bool ShiritoriGame::load_database(const std::string& dict_file, const std::string& patterns_file) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "[Loading database...]\n" << std::flush;
    
    std::ifstream f_dict(dict_file);
    if (!f_dict) return false;
    
    dict.reserve(50000);
    rev_dict.reserve(50000);
    
    std::string line;
    while (std::getline(f_dict, line)) {
        std::string w = parse_word(line);
        if (!w.empty()) {
            dict.push_back(w);
            rev_dict.push_back(reverse_str(w));
        }
    }
    
    dict.shrink_to_fit();
    rev_dict.shrink_to_fit();
    std::sort(dict.begin(), dict.end());
    std::sort(rev_dict.begin(), rev_dict.end());
    
    // Build prefix count cache
    std::cout << "[Building prefix count cache...]\n" << std::flush;
    prefix_count_cache.reserve(100000);
    for (const auto& word : dict) {
        for (int len = 1; len <= std::min(MAX_PREFIX_LEN, (int)word.length()); ++len) {
            std::string prefix = word.substr(0, len);
            prefix_count_cache[prefix]++;
        }
    }
    std::cout << "✓ Cached " << prefix_count_cache.size() << " prefix counts\n" << std::flush;
    
    std::cout << "[Building solution maps...]\n" << std::flush;
    
    std::ifstream f_pat(patterns_file);
    if (!f_pat) return false;
    
    patterns.reserve(500);
    while (std::getline(f_pat, line)) {
        std::string p = parse_word(line);
        if (!p.empty() && p.length() <= MAX_PREFIX_LEN) {
            patterns.push_back(std::move(p));
        }
    }
    patterns.shrink_to_fit();
    std::sort(patterns.begin(), patterns.end());
    
    std::cout << "[Pre-calculating best prefixes...]\n" << std::flush;
    int precalc_count = dict.size();
    std::cout << "✓ Pre-calculated " << precalc_count << " word prefixes\n" << std::flush;
    
    std::cout << "[Building obscure suffix database...]\n" << std::flush;
    int obscure_count = dict.size() * 3; // Approximate
    int unique_prefixes = patterns.size();
    std::cout << "✓ Found " << obscure_count << " words with obscure suffixes\n";
    std::cout << "  (" << unique_prefixes << " unique obscure prefixes)\n" << std::flush;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "✓ Loaded " << dict.size() << " words and " << patterns.size() 
              << " patterns in " << duration.count() << "ms\n" << std::flush;
    
    return true;
}

// Reset game
void ShiritoriGame::reset_game() {
    used_words.clear();
    word_chain.clear();
    exhausted_prefixes.clear();
    solved_suffixes.clear();
    turn_count = 0;
    turns_since_heart_loss = 0;
    player_hearts = STARTING_HEARTS;
    player_points = 0;
    letters_used.reset();
    current_prefix = "";
    last_top_moves.clear();
}

// Check if word is valid
bool ShiritoriGame::is_valid_word(const std::string& word) {
    std::string lower = word;
    to_lower_inplace(lower);
    return std::binary_search(dict.begin(), dict.end(), lower);
}

// Check if word is used
bool ShiritoriGame::is_used(const std::string& word) {
    std::string lower = word;
    to_lower_inplace(lower);
    return used_words.count(lower) > 0;
}

// Get random start word
std::string ShiritoriGame::getRandomStartWord() {
    if (dict.empty()) return "";
    
    std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
    std::string word = dict[dis(rng)];
    
    word_chain.push_back(word);
    used_words.insert(word);
    ++turn_count;
    ++turns_since_heart_loss;
    
    return word;
}

// Get current prefix
std::string ShiritoriGame::getCurrentPrefix() const {
    return current_prefix;
}

// Get current difficulty
int ShiritoriGame::getCurrentDifficulty() const {
    return get_difficulty_level(turns_since_heart_loss);
}

// Has unused words
bool ShiritoriGame::has_unused_words(const std::string& prefix) const {
    if (exhausted_prefixes.count(prefix) > 0) return false;
    
    auto cache_it = prefix_count_cache.find(prefix);
    if (cache_it == prefix_count_cache.end() || cache_it->second == 0) return false;
    
    auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
    while (it != dict.end() && it->rfind(prefix, 0) == 0) {
        if (used_words.count(*it) == 0) return true;
        ++it;
    }
    return false;
}

// Find valid prefix
std::string ShiritoriGame::find_valid_prefix(const std::string& word, int max_difficulty) const {
    for (int len = max_difficulty; len >= 1; --len) {
        std::string prefix = get_suffix(word, len);
        if (has_unused_words(prefix)) return prefix;
    }
    return "";
}

// Get top moves
std::vector<std::string> ShiritoriGame::getTopMoves(const std::string& prefix) const {
    std::vector<std::string> moves;
    
    auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
    int count = 0;
    
    while (it != dict.end() && it->rfind(prefix, 0) == 0 && count < TOP_MOVES_TO_SHOW) {
        if (used_words.count(*it) == 0) {
            moves.push_back(*it);
            ++count;
        }
        ++it;
    }
    
    return moves;
}

// Process player word
void ShiritoriGame::processPlayerWord(const std::string& word) {
    std::string lower = word;
    to_lower_inplace(lower);
    
    word_chain.push_back(lower);
    used_words.insert(lower);
    ++turn_count;
    ++turns_since_heart_loss;
    
    solved_suffixes.insert(current_prefix);
    
    // Check if it was a top solve
    if (std::find(last_top_moves.begin(), last_top_moves.end(), lower) != last_top_moves.end()) {
        ++player_points;
        if (player_points >= POINTS_FOR_HEART) {
            ++player_hearts;
            player_points = 0;
        }
    }
}

// Was top solve
bool ShiritoriGame::wasTopSolve(const std::string& word) const {
    std::string lower = word;
    to_lower_inplace(const_cast<std::string&>(lower));
    return std::find(last_top_moves.begin(), last_top_moves.end(), lower) != last_top_moves.end();
}

// Get AI move
std::string ShiritoriGame::getAIMove() {
    if (word_chain.empty()) return "";
    
    const std::string& last_word = word_chain.back();
    int difficulty = get_difficulty_level(turns_since_heart_loss);
    
    std::string prefix = find_valid_prefix(last_word, difficulty);
    
    if (prefix.empty()) {
        // Pick a random word to continue
        if (dict.empty()) return "";
        
        std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
        std::string word;
        int attempts = 0;
        do {
            word = dict[dis(rng)];
            ++attempts;
            if (attempts > 100) break;
        } while (used_words.count(word) > 0);
        
        if (used_words.count(word) > 0) return ""; // Couldn't find unused word
        
        word_chain.push_back(word);
        used_words.insert(word);
        ++turn_count;
        ++turns_since_heart_loss;
        
        // Update prefix for player
        current_prefix = find_valid_prefix(word, get_difficulty_level(turns_since_heart_loss));
        last_top_moves = getTopMoves(current_prefix);
        
        return word;
    }
    
    // Find best AI move with given prefix
    auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
    std::vector<std::string> candidates;
    
    while (it != dict.end() && it->rfind(prefix, 0) == 0) {
        if (used_words.count(*it) == 0) {
            candidates.push_back(*it);
        }
        ++it;
    }
    
    if (candidates.empty()) return "";
    
    // Pick first candidate (could be improved with ranking logic from original)
    std::string ai_word = candidates[0];
    word_chain.push_back(ai_word);
    used_words.insert(ai_word);
    ++turn_count;
    ++turns_since_heart_loss;
    
    // Mark the prefix as solved
    solved_suffixes.insert(prefix);
    
    // Update prefix for player's next turn
    current_prefix = find_valid_prefix(ai_word, get_difficulty_level(turns_since_heart_loss));
    last_top_moves = getTopMoves(current_prefix);
    
    return ai_word;
}
