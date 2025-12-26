#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <future>
#include <bitset>
#include <limits>
#include <termios.h>
#include <unistd.h>
#include <sstream>

// --- COLORS ---
const std::string C_RESET = "\033[0m";
const std::string C_CYAN = "\033[38;2;173;216;230m";
const std::string C_GREEN = "\033[38;2;100;200;100m";
const std::string C_YELLOW = "\033[38;2;255;215;0m";
const std::string C_RED = "\033[38;2;255;100;100m";
const std::string C_CREAM = "\033[38;2;255;253;208m";
const std::string C_GREY = "\033[38;2;150;150;150m";
const std::string C_MAGENTA = "\033[38;2;255;105;180m";
const std::string C_PROMPT = "\033[1;33m";
const std::string C_HEART = "\033[38;2;255;105;180m";
const std::string C_BLUE = "\033[38;2;100;149;237m";
const std::string C_ORANGE = "\033[38;2;255;165;0m";
const std::string C_GOLD = "\033[38;2;255;215;0m";

// --- CONFIG ---
const int MAX_PREFIX_LEN = 4;
const int MAX_SUFFIX_SCORE = 100;
const int PLAYER_TIMEOUT_SECONDS = 5;
const int STARTING_HEARTS = 2;
const int TOP_MOVES_TO_SHOW = 4;
const int OBSCURE_THRESHOLD = 15;
const int POINTS_FOR_HEART = 5;

// Blacklisted suffixes (trivial/too common)
const std::unordered_set<std::string> BLACKLIST_SUFFIXES = {
    "ness", "ally", "ses", "sis", "lity", "ties", "hies", 
    "phyll", "sts", "ossy", "uses", "oses", "tics", 
    "nist", "isms", "ity", "ions", "mian", "ies", "ers", "ing",
    "bias", "ias", "ous", "ful", "less", "able", "ible"
};

struct WordRank {
    std::string word;
    std::string suffix;
    int prefix_solutions_count;
    int max_solution_len;
    double obscurity_score;
    int difficulty_level;
    int creates_prefix_solutions;
    std::string creates_prefix;
    
    bool is_obscure_word;
    int obscure_suffix_length;
    double total_score;
    
    std::string best_creates_prefix;
    int best_creates_prefix_solutions;
    bool is_blacklisted;
    bool is_self_solving;
};

// --- HELPERS ---
inline std::string reverse_str(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}

inline void to_lower_inplace(std::string& s) {
    for (char& c : s) c = std::tolower(static_cast<unsigned char>(c));
}

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

bool get_input_with_locked_prefix(std::string& suffix, const std::string& prefix, int timeout_seconds) {
    enable_raw_mode();
    std::cout << C_CYAN << "→ " << C_YELLOW << prefix << C_RESET << std::flush;
    suffix.clear();
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        
        if (elapsed >= timeout_seconds) {
            disable_raw_mode();
            std::cout << "\n";
            return false;
        }
        
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                disable_raw_mode();
                std::cout << "\n";
                return true;
            } else if (c == 127 || c == 8) {
                if (!suffix.empty()) {
                    suffix.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            } else if (c == 3 || c == 4) {
                disable_raw_mode();
                std::cout << "\n";
                return false;
            } else if (std::isprint(c)) {
                suffix += c;
                std::cout << c << std::flush;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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

inline double calculateObscurityScore(const std::string& prefix, int solution_count,
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

inline bool rankerGlobal(const WordRank& a, const WordRank& b) {
    if (a.prefix_solutions_count != b.prefix_solutions_count) {
        return a.prefix_solutions_count < b.prefix_solutions_count;
    }
    if (std::abs(a.obscurity_score - b.obscurity_score) > 0.001) {
        return a.obscurity_score > b.obscurity_score;
    }
    if (a.max_solution_len != b.max_solution_len) {
        return a.max_solution_len > b.max_solution_len;
    }
    return a.word.length() > b.word.length();
}

// Enhanced ranker for top solves
inline bool rankerTopSolvesWithObscurity(const WordRank& a, const WordRank& b) {
    if (std::abs(a.total_score - b.total_score) > 0.001) {
        return a.total_score > b.total_score;
    }
    return a.word < b.word;
}

inline bool sortSolutionsByLength(const std::string& a, const std::string& b) {
    return a.length() != b.length() ? a.length() > b.length() : a < b;
}

void print_header(const std::string& title) {
    int content_width = title.length();
    int box_width = std::max(40, content_width + 4);
    
    std::string h_line;
    h_line.reserve(box_width * 3);
    for (int i = 0; i < box_width; ++i) h_line += "═";
    
    int padding = (box_width - title.length()) / 2;
    int padding_r = box_width - title.length() - padding;
    
    std::cout << C_CYAN << "╔" << h_line << "╗\n"
              << "║" << std::string(padding, ' ') << C_YELLOW << title << C_CYAN
              << std::string(padding_r, ' ') << "║\n"
              << "╚" << h_line << "╝\n" << C_RESET;
}

void countdown() {
    std::cout << C_YELLOW << "\nThe game starting in ";
    for (int i = 3; i >= 1; i--) {
        std::cout << i << ".. " << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    std::cout << "Go!\n\n" << C_RESET;
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

std::string display_hearts(int hearts) {
    std::string result;
    for (int i = 0; i < hearts; ++i) {
        result += C_HEART + "♥" + C_RESET;
    }
    return result;
}

std::string display_points(int points) {
    std::string result;
    for (int i = 0; i < points; ++i) {
        result += C_GOLD + "★" + C_RESET;
    }
    for (int i = points; i < POINTS_FOR_HEART; ++i) {
        result += C_GREY + "☆" + C_RESET;
    }
    return result;
}

class ShiritoriGame {
private:
    std::vector<std::string> dict;
    std::vector<std::string> rev_dict;
    std::vector<std::string> patterns;
    std::unordered_map<std::string, std::vector<std::string>> prefix_solutions;
    std::unordered_map<std::string, WordRank> word_to_rank_data;
    
    std::unordered_map<std::string, std::vector<std::string>> obscure_suffix_words;
    std::unordered_map<std::string, int> prefix_solution_counts;
    std::unordered_map<std::string, int> prefix_count_cache;
    
    std::unordered_set<std::string> used_words;
    std::unordered_set<std::string> exhausted_prefixes;
    std::unordered_set<std::string> solved_suffixes;  // ADD THIS LINE
    std::vector<std::string> word_chain;
    std::vector<std::string> failed_prefixes;
    std::vector<size_t> failure_positions;
    
    std::unordered_map<size_t, std::vector<std::string>> turn_top_solves;
    std::vector<size_t> player_turn_indices;
    std::unordered_map<std::string, std::pair<std::string, int>> player_word_to_prefix_faced;
    
    std::mt19937 rng;
    int turn_count;
    int turns_since_heart_loss;
    int player_hearts;
    int player_points;
    std::bitset<26> letters_used;
    
    // Check if word ends with ANY blacklisted suffix (not just the one we'd use)
    bool word_ends_with_blacklisted_suffix(const std::string& word) const {
        for (const auto& blacklisted : BLACKLIST_SUFFIXES) {
            if (word.length() >= blacklisted.length()) {
                std::string word_suffix = get_suffix(word, blacklisted.length());
                if (word_suffix == blacklisted) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // Check if a prefix is blacklisted
    bool is_prefix_blacklisted(const std::string& prefix) const {
        return BLACKLIST_SUFFIXES.count(prefix) > 0;
    }
    
    // Check if a prefix can be self-solved (word starting with prefix also ends with it)
    bool is_self_solving(const std::string& prefix) const {
        auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
        while (it != dict.end() && it->rfind(prefix, 0) == 0) {
            if (it->length() >= prefix.length()) {
                std::string suffix = get_suffix(*it, prefix.length());
                if (suffix == prefix) {
                    return true;
                }
            }
            ++it;
        }
        return false;
    }
    
    void build_obscure_suffix_database() {
        std::cout << C_CYAN << "[Building obscure suffix database...]\n" << C_RESET;
        
        obscure_suffix_words.clear();
        prefix_solution_counts.clear();
        prefix_solution_counts = prefix_count_cache;
        
        int obscure_count = 0;
        for (const auto& word : dict) {
            for (int len = 2; len <= std::min(MAX_PREFIX_LEN, (int)word.length()); ++len) {
                std::string suffix = get_suffix(word, len);
                
                auto it = prefix_solution_counts.find(suffix);
                if (it != prefix_solution_counts.end() && it->second <= OBSCURE_THRESHOLD) {
                    obscure_suffix_words[suffix].push_back(word);
                    obscure_count++;
                }
            }
        }
        
        std::cout << C_GREEN << "✓ Found " << obscure_count << " words with obscure suffixes\n" 
                  << C_GREY << "  (" << obscure_suffix_words.size() << " unique obscure prefixes)\n" << C_RESET;
    }
    
    bool has_unused_words(const std::string& prefix) const {
        if (exhausted_prefixes.count(prefix) > 0) return false;
        
        auto cache_it = prefix_count_cache.find(prefix);
        if (cache_it == prefix_count_cache.end() || cache_it->second == 0) return false;
        
        if (used_words.size() >= cache_it->second) {
            auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
            while (it != dict.end() && it->rfind(prefix, 0) == 0) {
                if (used_words.count(*it) == 0) return true;
                ++it;
            }
            return false;
        }
        return true;
    }
    
    std::string find_valid_prefix(const std::string& word, int max_difficulty) const {
        for (int len = max_difficulty; len >= 1; --len) {
            std::string prefix = get_suffix(word, len);
            if (has_unused_words(prefix)) return prefix;
        }
        return "";
    }
    
    int count_total_solutions_cached(const std::string& prefix) const {
        auto it = prefix_count_cache.find(prefix);
        return (it != prefix_count_cache.end()) ? it->second : 0;
    }
    
    int count_unused_solutions(const std::string& prefix) const {
        int count = 0;
        auto it = std::lower_bound(dict.begin(), dict.end(), prefix);
        while (it != dict.end() && it->rfind(prefix, 0) == 0) {
            if (used_words.count(*it) == 0) ++count;
            ++it;
        }
        return count;
    }
    
    std::pair<std::string, int> find_best_prefix_static_cached(const std::string& word) const {
        std::string best_prefix = "";
        int best_count = std::numeric_limits<int>::max();
        
        for (int len = std::min(MAX_PREFIX_LEN, (int)word.length()); len >= 1; --len) {
            std::string prefix = get_suffix(word, len);
            
            // Skip blacklisted prefixes
            if (is_prefix_blacklisted(prefix)) continue;
            
            // Skip self-solving prefixes
            if (is_self_solving(prefix)) continue;
            
            int solutions = count_total_solutions_cached(prefix);
            
            if (solutions > 0) {
                if (solutions < best_count || (solutions == best_count && prefix.length() > best_prefix.length())) {
                    best_prefix = prefix;
                    best_count = solutions;
                }
            }
        }
        
        // If no valid prefix found, try to return a non-blacklisted one even with 0 solutions
        if (best_prefix.empty()) {
            for (int len = std::min(MAX_PREFIX_LEN, (int)word.length()); len >= 1; --len) {
                std::string prefix = get_suffix(word, len);
                if (!is_prefix_blacklisted(prefix) && !is_self_solving(prefix)) {
                    return {prefix, 0};
                }
            }
            // Absolute fallback - return longest even if blacklisted
            return {get_suffix(word, std::min(MAX_PREFIX_LEN, (int)word.length())), 0};
        }
        return {best_prefix, best_count};
    }
    
    double calculate_word_score(const WordRank& wr) const {
        double score = 0.0;
        
        // Penalty for blacklisted or self-solving
        if (wr.is_blacklisted || wr.is_self_solving) {
            return -1000.0;  // Heavy penalty
        }
        
        // Penalty for 0 solutions (dead end)
        if (wr.creates_prefix_solutions == 0) {
            return -2000.0;  // Even heavier penalty
        }
        
        // Primary: Fewest solutions created
        if (wr.creates_prefix_solutions > 0) {
            score += 1000.0 / wr.creates_prefix_solutions;
        } else {
            score += 1000.0;
        }
        
        // Secondary: Obscurity bonus
        if (wr.is_obscure_word) {
            score += 500.0;
            score += wr.obscure_suffix_length * 50.0;
        }
        
        // Tertiary: Prefix length created
        score += wr.creates_prefix.length() * 20.0;
        
        // Quaternary: Word obscurity score
        score += wr.obscurity_score * 2.0;
        
        // Small bonus for longer words
        score += wr.word.length() * 1.0;
        
        return score;
    }
    
public:
    ShiritoriGame() : turn_count(0), turns_since_heart_loss(0),
                      player_hearts(STARTING_HEARTS), player_points(0) {
        auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        rng.seed(seed);
        letters_used.reset();
        failed_prefixes.reserve(10);
        failure_positions.reserve(10);
        exhausted_prefixes.reserve(10);
    }
    
    bool load_database(const std::string& dict_file, const std::string& patterns_file) {
        std::cout << C_CYAN << "\n[Loading database...]\n" << C_RESET;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::ifstream f_dict(dict_file);
        if (!f_dict) {
            std::cout << C_RED << "Error: Cannot open " << dict_file << "\n" << C_RESET;
            return false;
        }
        
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
        
        std::cout << C_CYAN << "[Building prefix count cache...]\n" << C_RESET;
        prefix_count_cache.reserve(100000);
        for (const auto& word : dict) {
            for (int len = 1; len <= std::min(MAX_PREFIX_LEN, (int)word.length()); ++len) {
                std::string prefix = word.substr(0, len);
                prefix_count_cache[prefix]++;
            }
        }
        std::cout << C_GREEN << "✓ Cached " << prefix_count_cache.size() << " prefix counts\n" << C_RESET;
        
        std::ifstream f_pat(patterns_file);
        if (!f_pat) {
            std::cout << C_RED << "Error: Cannot open " << patterns_file << "\n" << C_RESET;
            return false;
        }
        
        patterns.reserve(500);
        
        while (std::getline(f_pat, line)) {
            std::string p = parse_word(line);
            if (!p.empty() && p.length() <= MAX_PREFIX_LEN && !is_prefix_blacklisted(p)) {
                patterns.push_back(std::move(p));
            }
        }
        patterns.shrink_to_fit();
        std::sort(patterns.begin(), patterns.end());
        
        std::cout << C_CYAN << "[Building solution maps...]\n" << C_RESET;
        prefix_solutions.reserve(patterns.size());
        for (const auto& p : patterns) {
            auto it_pre = std::lower_bound(dict.begin(), dict.end(), p);
            std::vector<std::string> solutions;
            solutions.reserve(100);
            
            while (it_pre != dict.end() && it_pre->rfind(p, 0) == 0) {
                solutions.push_back(*it_pre);
                ++it_pre;
            }
            
            if (!solutions.empty()) {
                std::sort(solutions.begin(), solutions.end(), sortSolutionsByLength);
                prefix_solutions[p] = std::move(solutions);
            }
        }
        
        word_to_rank_data.reserve(dict.size() / 4);
        
        for (const auto& p : patterns) {
            const std::string rev_p = reverse_str(p);
            auto it_rev = std::lower_bound(rev_dict.begin(), rev_dict.end(), rev_p);
            
            if (it_rev == rev_dict.end() || it_rev->rfind(rev_p, 0) != 0) continue;
            
            int suffix_score = 0;
            std::vector<std::string> words_ending_with_p;
            words_ending_with_p.reserve(100);
            
            while (it_rev != rev_dict.end() && it_rev->rfind(rev_p, 0) == 0) {
                words_ending_with_p.push_back(reverse_str(*it_rev));
                ++suffix_score;
                ++it_rev;
            }
            
            if (suffix_score > 0 && suffix_score <= MAX_SUFFIX_SCORE) {
                auto prefix_it = prefix_solutions.find(p);
                int prefix_count = (prefix_it != prefix_solutions.end()) ? prefix_it->second.size() : 0;
                int longest_sol_len = (prefix_it != prefix_solutions.end() && !prefix_it->second.empty())
                                      ? prefix_it->second[0].length() : 0;
                
                double obscurity = calculateObscurityScore(p, prefix_count,
                                   (prefix_it != prefix_solutions.end()) ? prefix_it->second : std::vector<std::string>());
                
                for (const auto& word : words_ending_with_p) {
                    WordRank candidate = {word, p, prefix_count, longest_sol_len, obscurity, (int)p.length(), 
                                        0, "", false, 0, 0.0, "", 0, false, false};
                    
                    auto it = word_to_rank_data.find(word);
                    if (it == word_to_rank_data.end()) {
                        word_to_rank_data[word] = candidate;
                    } else if (rankerGlobal(candidate, it->second)) {
                        it->second = candidate;
                    }
                }
            }
        }
        
        std::cout << C_CYAN << "[Pre-calculating best prefixes...]\n" << C_RESET;
        int precalc_count = 0;
        for (auto& pair : word_to_rank_data) {
            auto best_info = find_best_prefix_static_cached(pair.first);
            pair.second.best_creates_prefix = best_info.first;
            pair.second.best_creates_prefix_solutions = best_info.second;
            pair.second.is_blacklisted = is_prefix_blacklisted(best_info.first);
            pair.second.is_self_solving = is_self_solving(best_info.first);
            ++precalc_count;
        }
        std::cout << C_GREEN << "✓ Pre-calculated " << precalc_count << " word prefixes\n" << C_RESET;
        
        build_obscure_suffix_database();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << C_GREEN << "✓ Loaded " << dict.size() << " words and " << patterns.size()
                  << " patterns in " << duration.count() << "ms\n" << C_RESET;
        return true;
    }
    
    void reset_game() {
        used_words.clear();
        word_chain.clear();
        failed_prefixes.clear();
        failure_positions.clear();
        exhausted_prefixes.clear();
        solved_suffixes.clear();
        turn_top_solves.clear();
        player_turn_indices.clear();
        player_word_to_prefix_faced.clear();
        turn_count = 0;
        turns_since_heart_loss = 0;
        player_hearts = STARTING_HEARTS;
        player_points = 0;
        letters_used.reset();
    }
    
    inline bool is_valid_word(const std::string& word) {
        std::string lower = word;
        to_lower_inplace(lower);
        return std::binary_search(dict.begin(), dict.end(), lower);
    }
    
    inline bool is_used(const std::string& word) {
        std::string lower = word;
        to_lower_inplace(lower);
        return used_words.count(lower) > 0;
    }
    
    void mark_letter_used(char first_letter) {
        if (first_letter >= 'a' && first_letter <= 'z') {
            letters_used.set(first_letter - 'a');
        }
        
        if (letters_used.all() && player_hearts > 0) {
            ++player_hearts;
            std::cout << C_GREEN << "🎉 All 26 letters used! +1 heart!\n" << C_RESET;
            letters_used.reset();
        }
    }
    
    std::vector<WordRank> get_top_ai_moves(const std::string& required_prefix, int top_n = TOP_MOVES_TO_SHOW) {
        std::vector<WordRank> candidates;
        candidates.reserve(100);
        
        auto it = std::lower_bound(dict.begin(), dict.end(), required_prefix);
        
        while (it != dict.end() && it->rfind(required_prefix, 0) == 0) {
            if (used_words.count(*it) == 0) {
                auto rank_it = word_to_rank_data.find(*it);
                WordRank candidate;
                
                if (rank_it != word_to_rank_data.end()) {
                    candidate = rank_it->second;
                } else {
                    candidate = {*it, "", 999, 0, 0.0, (int)required_prefix.length(), 
                               0, "", false, 0, 0.0, "", 0, false, false};
                }
                
                candidates.push_back(candidate);
            }
            ++it;
        }
        
        if (candidates.empty()) return {};
        
        for (auto& candidate : candidates) {
            if (!candidate.best_creates_prefix.empty()) {
                // Use pre-calculated data from word_to_rank_data
                candidate.creates_prefix = candidate.best_creates_prefix;
                candidate.creates_prefix_solutions = candidate.best_creates_prefix_solutions;
                // is_blacklisted and is_self_solving are already set from word_to_rank_data
            } else {
                // Word not in rank data, calculate on the fly
                auto prefix_info = find_best_prefix_static_cached(candidate.word);
                candidate.creates_prefix = prefix_info.first;
                candidate.creates_prefix_solutions = prefix_info.second;
                candidate.is_blacklisted = is_prefix_blacklisted(candidate.creates_prefix);
                candidate.is_self_solving = is_self_solving(candidate.creates_prefix);
            }
            
            candidate.is_obscure_word = false;
            candidate.obscure_suffix_length = 0;
            for (int len = MAX_PREFIX_LEN; len >= 2; --len) {
                std::string suffix = get_suffix(candidate.word, len);
                if (obscure_suffix_words.count(suffix) > 0) {
                    candidate.is_obscure_word = true;
                    candidate.obscure_suffix_length = len;
                    break;
                }
            }
            
            candidate.total_score = calculate_word_score(candidate);
        }
        
        // Filter out words that:
        // 1. Have a blacklisted/self-solving best prefix, OR
        // 2. End with ANY blacklisted suffix (even longer than MAX_PREFIX_LEN), OR
        // 3. Create a prefix with 0 solutions (dead end)
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [this](const WordRank& wr) {
                    return wr.is_blacklisted || wr.is_self_solving || 
                           word_ends_with_blacklisted_suffix(wr.word) ||
                           wr.creates_prefix_solutions == 0;
                }),
            candidates.end()
        );
        
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
              [this](const WordRank& wr) {
              // Only filter if the BEST creates_prefix is solved
              return solved_suffixes.count(wr.creates_prefix) > 0;
              }),
            candidates.end()
            );

        if (candidates.empty()) return {};

        std::sort(candidates.begin(), candidates.end(), rankerTopSolvesWithObscurity);

        if (candidates.empty()) return {};

        std::sort(candidates.begin(), candidates.end(), rankerTopSolvesWithObscurity);
        
        if (candidates.size() > top_n) {
            candidates.resize(top_n);
        }
        
        return candidates;
    }
    
    std::string get_ai_move(const std::string& required_prefix) {
        std::vector<WordRank> candidates;
        candidates.reserve(100);
        
        auto it = std::lower_bound(dict.begin(), dict.end(), required_prefix);
        
        while (it != dict.end() && it->rfind(required_prefix, 0) == 0) {
            if (used_words.count(*it) == 0) {
                auto rank_it = word_to_rank_data.find(*it);
                if (rank_it != word_to_rank_data.end()) {
                    candidates.push_back(rank_it->second);
                } else {
                    WordRank default_rank = {*it, "", 999, 0, 0.0, (int)required_prefix.length(), 
                                           0, "", false, 0, 0.0, "", 0, false, false};
                    candidates.push_back(default_rank);
                }
            }
            ++it;
        }
        
        if (candidates.empty()) return "";
        
        std::sort(candidates.begin(), candidates.end(), rankerGlobal);
        
        std::vector<WordRank> viable_candidates;
        
        for (const auto& candidate : candidates) {
            int player_difficulty = get_difficulty_level(turns_since_heart_loss + 1);
            std::string viable_player_prefix = find_valid_prefix(candidate.word, player_difficulty);
            
            if (viable_player_prefix.empty()) continue;
            
            used_words.insert(candidate.word);
            bool ai_can_continue_after_player = false;
            
            auto player_it = std::lower_bound(dict.begin(), dict.end(), viable_player_prefix);
            
            while (player_it != dict.end() && player_it->rfind(viable_player_prefix, 0) == 0) {
                if (used_words.count(*player_it) == 0) {
                    int ai_next_difficulty = get_difficulty_level(turns_since_heart_loss + 2);
                    std::string ai_next_prefix = find_valid_prefix(*player_it, ai_next_difficulty);
                    
                    if (!ai_next_prefix.empty()) {
                        ai_can_continue_after_player = true;
                        break;
                    }
                }
                ++player_it;
            }
            
            used_words.erase(candidate.word);
            
            if (ai_can_continue_after_player) {
                viable_candidates.push_back(candidate);
            }
        }
        
        if (viable_candidates.empty()) return "";
        
        size_t start_idx = 0;
        while (start_idx < viable_candidates.size()) {
            int current_count = viable_candidates[start_idx].prefix_solutions_count;
            size_t end_idx = start_idx;
            
            while (end_idx < viable_candidates.size() &&
                   viable_candidates[end_idx].prefix_solutions_count == current_count) {
                ++end_idx;
            }
            
            size_t group_size = end_idx - start_idx;
            size_t shuffle_size = std::min(group_size, static_cast<size_t>(50));
            if (shuffle_size > 1) {
                std::shuffle(viable_candidates.begin() + start_idx,
                           viable_candidates.begin() + start_idx + shuffle_size, rng);
            }
            
            start_idx = end_idx;
        }
        
        return viable_candidates[0].word;
    }
    
    void display_final_chain() {
        std::cout << "\n" << C_CYAN << "═══════════════════════════════════════════════════\n" << C_RESET;
        std::cout << C_YELLOW << "Final Chain (" << turn_count << " turns):\n\n" << C_RESET;
        
        for (size_t i = 0; i < word_chain.size(); ++i) {
            std::cout << (i % 2 == 1 ? C_CYAN : C_MAGENTA) << word_chain[i] << C_RESET;
            if (i < word_chain.size() - 1) {
                std::cout << C_GREY << " → " << C_RESET;
            }
        }
        std::cout << "\n";
        
        if (!failed_prefixes.empty()) {
            std::cout << "\n" << C_RED << "Failed Prefixes:\n" << C_RESET;
            for (const auto& prefix : failed_prefixes) {
                std::cout << C_RED << " • " << prefix << C_RESET << "\n";
            }
        }
        
        if (!player_turn_indices.empty()) {
            std::vector<std::pair<std::string, bool>> player_choices;
            
            for (size_t player_idx : player_turn_indices) {
                if (player_idx < word_chain.size()) {
                    const std::string& player_word = word_chain[player_idx];
                    
                    bool was_top_solve = false;
                    auto top_it = turn_top_solves.find(player_idx);
                    if (top_it != turn_top_solves.end()) {
                        const auto& shown_solves = top_it->second;
                        was_top_solve = (std::find(shown_solves.begin(), shown_solves.end(), player_word) != shown_solves.end());
                    }
                    
                    player_choices.push_back({player_word, was_top_solve});
                }
            }
            
            if (!player_choices.empty()) {
                std::cout << "\n" << C_CYAN << "📊 Your Choices:\n" << C_RESET;
                for (const auto& choice : player_choices) {
                    auto prefix_it = player_word_to_prefix_faced.find(choice.first);
                    if (prefix_it != player_word_to_prefix_faced.end()) {
                        int solution_count = prefix_it->second.second;
                        const std::string& created_suffix = prefix_it->second.first;
                        
                        if (choice.second) {
                            std::cout << C_CYAN << " ✓ " << choice.first << C_GREY 
                                      << " (\"" << created_suffix << "\" / " << solution_count 
                                      << (solution_count == 1 ? " solution)" : " solutions)") << C_RESET << "\n";
                        } else {
                            std::cout << C_GREY << " ⚡ " << choice.first << C_RESET << "\n";
                        }
                    }
                }
            }
            
            std::vector<std::tuple<std::string, std::string, int>> top_solve_picks;
            
            for (size_t player_idx : player_turn_indices) {
                if (player_idx < word_chain.size()) {
                    const std::string& player_word = word_chain[player_idx];
                    
                    auto prefix_it = player_word_to_prefix_faced.find(player_word);
                    if (prefix_it != player_word_to_prefix_faced.end()) {
                        const std::string& created_suffix = prefix_it->second.first;
                        int unused_solution_count = prefix_it->second.second;
                        
                        if (unused_solution_count >= 1 && unused_solution_count <= 10) {
                            top_solve_picks.push_back(std::make_tuple(player_word, created_suffix, unused_solution_count));
                        }
                    }
                }
            }
            
            if (!top_solve_picks.empty()) {
                std::sort(top_solve_picks.begin(), top_solve_picks.end(), 
                    [](const std::tuple<std::string, std::string, int>& a, const std::tuple<std::string, std::string, int>& b) {
                        if (std::get<2>(a) != std::get<2>(b)) {
                            return std::get<2>(a) < std::get<2>(b);
                        }
                        return std::get<0>(a) < std::get<0>(b);
                    });
                
                std::cout << "\n" << C_GREEN << "✨ Your Top Solves (≤10 solutions created):\n" << C_RESET;
                for (const auto& pick : top_solve_picks) {
                    std::cout << C_GREEN << " ✓ " << std::get<0>(pick) << C_GREY 
                              << " (\"" << std::get<1>(pick) << "\" / " << std::get<2>(pick) 
                              << (std::get<2>(pick) == 1 ? " solution)" : " solutions)") << C_RESET << "\n";
                }
            }
        }
        
        std::cout << "\n" << C_CYAN << "═══════════════════════════════════════════════════\n" << C_RESET;
    }
    
    void display_status() {
        std::cout << C_GREY << "Hearts: " << C_RESET << display_hearts(player_hearts);
        if (player_points > 0) {
            std::cout << C_GREY << "  Points: " << C_RESET << display_points(player_points);
        }
        std::cout << "\n\n";
    }
    
    void play() {
        print_header("HIYODORI'S SHIRITORI");
        
        std::cout << "\n" << C_CYAN << "Do you wish to play a game? " << C_PROMPT << "[y/n]: " << C_RESET;
        std::string response;
        std::getline(std::cin, response);
        to_lower_inplace(response);
        
        if (response != "y" && response != "yes") {
            std::cout << C_GREY << "Maybe next time!\n" << C_RESET;
            return;
        }
        
        countdown();
        
        std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
        std::string first_word = dict[dis(rng)];
        
        word_chain.reserve(50);
        word_chain.push_back(first_word);
        used_words.insert(first_word);
        ++turn_count;
        ++turns_since_heart_loss;
        
        std::cout << C_MAGENTA << "AI: " << C_CREAM << first_word << C_RESET << "\n\n";
        mark_letter_used(first_word[0]);
        
        while (player_hearts > 0) {
            const std::string& last_word = word_chain.back();
            int difficulty = get_difficulty_level(turns_since_heart_loss);
            
            std::string required_prefix = find_valid_prefix(last_word, difficulty);
            
            if (required_prefix.empty()) {
                for (int len = difficulty; len >= 1; --len) {
                    exhausted_prefixes.insert(get_suffix(last_word, len));
                }
                --player_hearts;
                player_points = 0;
                turns_since_heart_loss = 0;
                std::cout << C_RED << "💔 No valid moves available! Lost 1 heart! (" << player_hearts << " remaining)\n\n" << C_RESET;
                if (player_hearts <= 0) {
                    std::cout << C_RED << "\nGame Over! AI wins!\n" << C_RESET;
                    break;
                }
                continue;
            }
            
            display_status();
            std::cout << C_GREY << "Difficulty: " << difficulty << "/4\n" << C_RESET;
            std::cout << C_YELLOW << required_prefix << C_RESET << "\n\n";
            
            auto top_moves_for_turn = get_top_ai_moves(required_prefix, TOP_MOVES_TO_SHOW * 2);
            std::vector<std::string> top_words_list;
            for (const auto& move : top_moves_for_turn) {
                top_words_list.push_back(move.word);
            }
            turn_top_solves[word_chain.size()] = top_words_list;
            
            std::string player_suffix;
            std::string player_word;
            bool valid_input = false;
            bool timed_out = false;
            
            auto turn_start_time = std::chrono::steady_clock::now();
            
            while (!valid_input && player_hearts > 0 && !timed_out) {
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    current_time - turn_start_time).count();
                int remaining_time = PLAYER_TIMEOUT_SECONDS - elapsed_seconds;
                
                if (remaining_time <= 0) {
                    std::cout << C_RED << "⏱ Time's up!\n" << C_RESET;
                    --player_hearts;
                    player_points = 0;
                    turns_since_heart_loss = 0;
                    failed_prefixes.push_back(required_prefix);
                    failure_positions.push_back(word_chain.size());
                    exhausted_prefixes.insert(required_prefix);
                    std::cout << C_RED << "💔 Lost 1 heart! (" << player_hearts << " remaining)\n\n" << C_RESET;
                    timed_out = true;
                    break;
                }
                
                if (!get_input_with_locked_prefix(player_suffix, required_prefix, remaining_time)) {
                    std::cout << C_RED << "⏱ Time's up!\n" << C_RESET;
                    --player_hearts;
                    player_points = 0;
                    turns_since_heart_loss = 0;
                    failed_prefixes.push_back(required_prefix);
                    failure_positions.push_back(word_chain.size());
                    exhausted_prefixes.insert(required_prefix);
                    std::cout << C_RED << "💔 Lost 1 heart! (" << player_hearts << " remaining)\n\n" << C_RESET;
                    timed_out = true;
                    break;
                }
                
                player_word = required_prefix + player_suffix;
                to_lower_inplace(player_word);
                
                if (player_word == required_prefix + "quit" || player_word == required_prefix + "exit") {
                    std::cout << C_YELLOW << "\nYou surrendered. AI wins!\n" << C_RESET;
                    failed_prefixes.push_back(required_prefix);
                    failure_positions.push_back(word_chain.size());
                    exhausted_prefixes.insert(required_prefix);
                    player_hearts = 0;
                    break;
                }
                
                if (!is_valid_word(player_word)) {
                    std::cout << C_RED << "✗ Not in dictionary! Try again.\n" << C_RESET;
                    continue;
                }
                
                if (is_used(player_word)) {
                    std::cout << C_RED << "✗ Word already used! Try again.\n" << C_RESET;
                    continue;
                }
                
                word_chain.push_back(player_word);
                used_words.insert(player_word);
                ++turn_count;
                ++turns_since_heart_loss;
                mark_letter_used(player_word[0]);
                
                player_turn_indices.push_back(word_chain.size() - 1);
                
                std::string created_suffix;
                int created_suffix_solutions;
                
                auto rank_it = word_to_rank_data.find(player_word);
                if (rank_it != word_to_rank_data.end() && !rank_it->second.best_creates_prefix.empty()) {
                    created_suffix = rank_it->second.best_creates_prefix;
                    created_suffix_solutions = rank_it->second.best_creates_prefix_solutions;
                } else {
                    auto prefix_info = find_best_prefix_static_cached(player_word);
                    created_suffix = prefix_info.first;
                    created_suffix_solutions = prefix_info.second;
                }
                
                player_word_to_prefix_faced[player_word] = {created_suffix, created_suffix_solutions};
                
                std::cout << C_GREEN << "✓ Valid!\n" << C_RESET;

                solved_suffixes.insert(required_prefix);
                
                auto top_it = turn_top_solves.find(word_chain.size() - 1);
                bool was_top_solve = false;
                if (top_it != turn_top_solves.end()) {
                    const auto& shown_solves = top_it->second;
                    was_top_solve = (std::find(shown_solves.begin(), shown_solves.end(), player_word) != shown_solves.end());
                }
                
                if (was_top_solve) {
                    ++player_points;
                    std::cout << C_GOLD << "⭐ TOP SOLVE! +1 point! (" 
                              << player_points << "/" << POINTS_FOR_HEART << ")\n" << C_RESET;
                    
                    if (player_points >= POINTS_FOR_HEART) {
                        ++player_hearts;
                        player_points = 0;
                        std::cout << C_GREEN << "🎉 EARNED A HEART! (Now at " << player_hearts << " hearts)\n" << C_RESET;
                    }
                }
                
                std::cout << "\n";
                
                if (!top_moves_for_turn.empty()) {
                    std::cout << C_BLUE << "top solves: ";
                    int shown = 0;
                    for (size_t i = 0; i < top_moves_for_turn.size() && shown < TOP_MOVES_TO_SHOW; ++i) {
                        bool is_used = (used_words.count(top_moves_for_turn[i].word) > 0);
                        
                        if (shown > 0) std::cout << C_GREY << ", " << C_RESET;
                        
                        if (is_used) {
                            std::cout << C_GREY << top_moves_for_turn[i].word << " [used]" << C_RESET;
                        } else if (top_moves_for_turn[i].is_obscure_word) {
                            std::cout << C_ORANGE << top_moves_for_turn[i].word << C_RESET;
                        } else {
                            std::cout << C_CREAM << top_moves_for_turn[i].word << C_RESET;
                        }
                        
                        std::cout << C_GREY << " (" << top_moves_for_turn[i].creates_prefix_solutions << ")" << C_RESET;
                        shown++;
                    }
                    std::cout << "\n";
                } else {
                    std::cout << C_BLUE << "top solves: none found\n" << C_RESET;
                }
                
                std::cout << "\n";
                valid_input = true;
            }
            
            if (player_hearts <= 0) {
                std::cout << C_RED << "\nGame Over! AI wins!\n" << C_RESET;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                break;
            }
            
            if (timed_out) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                
                std::cout << C_GREY << "AI continues with a new word...\n" << C_RESET;
                std::this_thread::sleep_for(std::chrono::milliseconds(600));
                
                const std::string& last_successful_word = word_chain.back();
                
                std::string ai_prefix = find_valid_prefix(last_successful_word, 4);
                std::string ai_word;
                
                if (!ai_prefix.empty()) {
                    ai_word = get_ai_move(ai_prefix);
                }
                
                if (ai_word.empty()) {
                  turns_since_heart_loss = 0;
                  std::cout << C_CYAN << "AI picks a new word to continue...\n" << C_RESET;
                  std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
                  // Find a word that has valid continuations
                  std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
                  int attempts = 0;
                  do {
                    ai_word = dict[dis(rng)];
                    ++attempts;
                    if (attempts > 100) break; // Prevent infinite loop
                  } while (used_words.count(ai_word) > 0 || find_valid_prefix(ai_word, 1).empty());
    
                  // If still can't find a valid word, game over
                  if (find_valid_prefix(ai_word, 1).empty()) {
                    std::cout << C_RED << "No valid continuations possible. Game Over!\n" << C_RESET;
                    player_hearts = 0;
                    break;
                  }
                }
                
                word_chain.push_back(ai_word);
                used_words.insert(ai_word);
                ++turn_count;
                ++turns_since_heart_loss;
                mark_letter_used(ai_word[0]);

                // Mark the AI's prefix as solved since AI successfully used it
                if (!ai_prefix.empty()) {
                  solved_suffixes.insert(ai_prefix);  // ADD THIS LINE
                }
                
                std::cout << C_MAGENTA << "AI: " << C_CREAM << ai_word << C_RESET;
                
                auto rank_it = word_to_rank_data.find(ai_word);
                if (rank_it != word_to_rank_data.end()) {
                    std::cout << C_GREY << " (suffix: '" << rank_it->second.suffix
                              << "', solutions: " << rank_it->second.prefix_solutions_count << ")" << C_RESET;
                }
                std::cout << "\n\n";
                
                continue;
            }
            
            difficulty = get_difficulty_level(turns_since_heart_loss);
            
            std::string ai_prefix = find_valid_prefix(player_word, difficulty);
            
            std::cout << C_GREY << "AI is thinking... " << C_RESET;
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            std::cout << "\n";
            
            std::string ai_word;
            
            if (!ai_prefix.empty()) {
                ai_word = get_ai_move(ai_prefix);
            }
            
            if (ai_word.empty()) {
                turns_since_heart_loss = 0;
                std::cout << C_CYAN << "AI picks a new word to continue...\n" << C_RESET;
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                
                std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
                ai_word = dict[dis(rng)];
                while (used_words.count(ai_word) > 0) {
                    ai_word = dict[dis(rng)];
                }
            }
            
            word_chain.push_back(ai_word);
            used_words.insert(ai_word);
            ++turn_count;
            ++turns_since_heart_loss;
            mark_letter_used(ai_word[0]);
            
            std::cout << C_MAGENTA << "AI: " << C_CREAM << ai_word << C_RESET;
            
            auto rank_it = word_to_rank_data.find(ai_word);
            if (rank_it != word_to_rank_data.end()) {
                std::cout << C_GREY << " (suffix: '" << rank_it->second.suffix
                          << "', solutions: " << rank_it->second.prefix_solutions_count << ")" << C_RESET;
            }
            std::cout << "\n\n";
        }
        
        display_final_chain();
        disable_raw_mode();
    }
};

int main() {
    std::atexit(disable_raw_mode);
    
    std::cout << "\033[2J\033[1;1H";
    print_header("HIYODORI'S SHIRITORI");
    
    std::cout << "\n" << C_CYAN << "Use default file paths? " << C_PROMPT << "[y/n]: " << C_RESET;
    std::string use_defaults;
    std::getline(std::cin, use_defaults);
    to_lower_inplace(use_defaults);
    
    std::string dict_path, patterns_path;
    
    if (use_defaults == "y" || use_defaults == "yes") {
        dict_path = "/home/hiyodori/Words/Dictionaries/Last_Letter/last_letter.txt";
        patterns_path = "/home/hiyodori/Words/Dictionaries/Last_Letter/rare_prefix.txt";
        std::cout << C_GREEN << "✓ Using default files\n" << C_RESET;
    } else {
        std::cout << "\n" << C_CYAN << "Dictionary file path: " << C_RESET;
        std::getline(std::cin, dict_path);
        
        std::cout << C_CYAN << "Patterns file path: " << C_RESET;
        std::getline(std::cin, patterns_path);
    }
    
    ShiritoriGame game;
    
    if (!game.load_database(dict_path, patterns_path)) {
        std::cout << C_RED << "Failed to load database files.\n" << C_RESET;
        return 1;
    }
    
    bool play_again = true;
    while (play_again) {
        game.play();
        
        std::cout << "\n" << C_CYAN << "Do you wish to play again? " << C_PROMPT << "[y/n]: " << C_RESET;
        std::string response;
        std::getline(std::cin, response);
        to_lower_inplace(response);
        
        if (response != "y" && response != "yes") {
            play_again = false;
            std::cout << C_YELLOW << "\nThanks for playing!\n" << C_RESET;
        } else {
            game.reset_game();
            std::cout << "\033[2J\033[1;1H";
        }
    }
    
    return 0;
}
