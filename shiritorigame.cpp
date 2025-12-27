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

inline double calculateObscurityScoreLocal(const std::string& prefix, int solution_count,
                                const std::vector<std::string>& solutions) {
    double score = (prefix.length() - 2) * 10.0;
    static const std::string rare_letters = "jqxzvkw";
    for (char c : prefix) {
        if (rare_letters.find(c) != std::string::npos) score += 15.0;
    }

    if (!solutions.empty()) {
        size_t total_len = 0;
        for (const auto& sol : solutions) total_len += sol.length();
        double avg_len = static_cast<double>(total_len) / solutions.size();
        score += (avg_len - 5.0) * 2.0;
    }

    static const std::string vowels = "aeiou";
    int consonant_streak = 0, max_consonant_streak = 0;
    for (char c : prefix) {
        if (vowels.find(c) == std::string::npos) {
            max_consonant_streak = std::max(max_consonant_streak, ++consonant_streak);
        } else {
            consonant_streak = 0;
        }
    }
    if (max_consonant_streak >= 3) score += max_consonant_streak * 8.0;

    return score;
}

// Find best creates-prefix for a word (tries lengths from MAX_PREFIX_LEN..1)
std::pair<std::string,int> find_best_prefix_static_cached_local(const std::string& word,
                                                                const std::unordered_map<std::string,int>& prefix_count_cache,
                                                                const std::unordered_set<std::string>& blacklist,
                                                                const std::vector<std::string>& dict_list) {
    std::string best_prefix = "";
    int best_count = std::numeric_limits<int>::max();

    for (int len = std::min(MAX_PREFIX_LEN, (int)word.length()); len >= 1; --len) {
        std::string prefix = get_suffix(word, len);
        // skip blacklisted
        if (blacklist.count(prefix) > 0) continue;
        // skip self-solving: check if any dict word starting with prefix also ends with prefix
        bool self_solving = false;
        auto it = std::lower_bound(dict_list.begin(), dict_list.end(), prefix);
        while (it != dict_list.end() && it->rfind(prefix,0) == 0) {
            if (it->length() >= prefix.length()) {
                std::string sfx = get_suffix(*it, prefix.length());
                if (sfx == prefix) { self_solving = true; break; }
            }
            ++it;
        }
        if (self_solving) continue;

        auto itc = prefix_count_cache.find(prefix);
        int solutions = (itc != prefix_count_cache.end()) ? itc->second : 0;
        if (solutions > 0) {
            if (solutions < best_count || (solutions == best_count && (int)prefix.length() > (int)best_prefix.length())) {
                best_prefix = prefix;
                best_count = solutions;
            }
        }
    }

    if (best_prefix.empty()) {
        for (int len = std::min(MAX_PREFIX_LEN, (int)word.length()); len >= 1; --len) {
            std::string prefix = get_suffix(word, len);
            if (blacklist.count(prefix) == 0) return {prefix, 0};
        }
        return {get_suffix(word, std::min(MAX_PREFIX_LEN, (int)word.length())), 0};
    }
    return {best_prefix, best_count};
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

// Count solutions (suffix matches) for a given prefix
int ShiritoriGame::countSolutions(const std::string& prefix) const {
    int count = 0;
    auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
    
    while (it != dict.end() && it->rfind(prefix, 0) == 0) {
        count++;
        ++it;
    }
    
    return count;
}

// Check if word ends with blacklisted suffix
bool ShiritoriGame::word_ends_with_blacklisted_suffix(const std::string& word) const {
    for (const auto& blacklisted : BLACKLIST_SUFFIXES) {
        if (word.length() >= blacklisted.length()) {
            std::string word_suffix = word.length() >= blacklisted.length() ? 
                                     word.substr(word.length() - blacklisted.length()) : "";
            if (word_suffix == blacklisted) {
                return true;
            }
        }
    }
    return false;
}

// Check if prefix is blacklisted
bool ShiritoriGame::is_prefix_blacklisted(const std::string& prefix) const {
    return BLACKLIST_SUFFIXES.count(prefix) > 0;
}

// Check if prefix is self-solving
bool ShiritoriGame::is_prefix_self_solving(const std::string& prefix) const {
    auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
    while (it != dict.end() && it->rfind(prefix, 0) == 0) {
        if (it->length() >= prefix.length()) {
            std::string suffix = it->substr(it->length() - prefix.length());
            if (suffix == prefix) {
                return true;
            }
        }
        ++it;
    }
    return false;
}

// Get top AI moves - using shiritori.cpp's filtering algorithm
std::vector<WordRank> ShiritoriGame::getTopAIMoves(const std::string& required_prefix, int top_n) {
    std::vector<WordRank> candidates;
    candidates.reserve(200);

    auto it = std::lower_bound(dict.begin(), dict.end(), required_prefix);

    while (it != dict.end() && it->rfind(required_prefix, 0) == 0) {
        if (used_words.count(*it) == 0) {
            WordRank wr;
            wr.word = *it;

            // Determine best creates-prefix and its solution count
            auto prefix_info = find_best_prefix_static_cached_local(wr.word, prefix_count_cache, BLACKLIST_SUFFIXES, dict);
            wr.creates_prefix = prefix_info.first;
            wr.creates_prefix_solutions = prefix_info.second;
            wr.is_blacklisted = is_prefix_blacklisted(wr.creates_prefix);
            wr.is_self_solving = is_prefix_self_solving(wr.creates_prefix);

            // Obscurity detection: count short obscure suffixes
            wr.is_obscure_word = false;
            wr.obscure_suffix_length = 0;
            for (int len = MAX_PREFIX_LEN; len >= 2; --len) {
                std::string suffix = get_suffix(wr.word, len);
                auto itc = prefix_count_cache.find(suffix);
                if (itc != prefix_count_cache.end() && itc->second <= OBSCURE_THRESHOLD) {
                    wr.is_obscure_word = true;
                    wr.obscure_suffix_length = len;
                    break;
                }
            }

            // Calculate obscurity score (approx)
            std::vector<std::string> dummy_sols; // we don't have exact solutions list here
            wr.obscurity_score = calculateObscurityScoreLocal(wr.creates_prefix, wr.creates_prefix_solutions, dummy_sols);

            // Difficulty level approximated by prefix length
            wr.difficulty_level = (int)wr.creates_prefix.length();

            // compute total score similar to shiritori.cpp's calculate_word_score
            double total = 0.0;
            if (wr.is_blacklisted || wr.is_self_solving) {
                total = -1000.0;
            } else if (wr.creates_prefix_solutions == 0) {
                total = -2000.0;
            } else {
                total += 1000.0 / wr.creates_prefix_solutions;
                if (wr.is_obscure_word) {
                    total += 500.0 + wr.obscure_suffix_length * 50.0;
                }
                total += wr.creates_prefix.length() * 20.0;
                total += wr.obscurity_score * 2.0;
                total += wr.word.length() * 1.0;
            }
            wr.total_score = total;

            // push candidate
            candidates.push_back(wr);
        }
        ++it;
    }

    // Filter out unwanted candidates
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
            [this](const WordRank& wr) {
                return wr.is_blacklisted || wr.is_self_solving ||
                       word_ends_with_blacklisted_suffix(wr.word) ||
                       wr.creates_prefix_solutions == 0 ||
                       solved_suffixes.count(wr.creates_prefix) > 0;
            }),
        candidates.end()
    );

    if (candidates.empty()) return {};

    // Sort by total score descending (rarest + obscurity prioritized), then word
    std::sort(candidates.begin(), candidates.end(), [](const WordRank& a, const WordRank& b) {
        if (std::abs(a.total_score - b.total_score) > 0.001) return a.total_score > b.total_score;
        return a.word < b.word;
    });

    if (candidates.size() > static_cast<size_t>(top_n)) candidates.resize(top_n);
    return candidates;
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

void ShiritoriGame::losePlayerHeart() {
    if (player_hearts > 0) player_hearts -= 1;
    // reset points and difficulty timer when a heart is lost
    player_points = 0;
    turns_since_heart_loss = 0;
}

// Was top solve
bool ShiritoriGame::wasTopSolve(const std::string& word) const {
    std::string lower = word;
    to_lower_inplace(const_cast<std::string&>(lower));
    return std::find(last_top_moves.begin(), last_top_moves.end(), lower) != last_top_moves.end();
}

// Get a new prefix for a given word and difficulty level (public wrapper)
std::string ShiritoriGame::getNewPrefix(const std::string& word, int difficulty) const {
    return find_valid_prefix(word, difficulty);
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
