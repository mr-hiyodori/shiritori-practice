#include "shiritorigame.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iostream>
#include <map>
#include <set>

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

  if (solution_count > 0) {
    score += (100.0 / solution_count);
  }

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

// Find best creates-prefix for a word
std::pair<std::string,int> find_best_prefix_static_cached_local(const std::string& word,
    const std::unordered_map<std::string,int>& prefix_count_cache,
    const std::unordered_set<std::string>& blacklist,
    const std::vector<std::string>& dict_list) {
  std::string best_prefix = "";
  int best_count = std::numeric_limits<int>::max();

  for (int len = std::min(MAX_PREFIX_LEN, (int)word.length()); len >= 1; --len) {
    std::string prefix = get_suffix(word, len);
    if (blacklist.count(prefix) > 0) continue;

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

// blacklist
bool ShiritoriGame::word_ends_with_blacklisted_suffix(const std::string& word) const {
  for (const auto& suffix : BLACKLIST_SUFFIXES) {
    if (word.length() >= suffix.length()) {
      if (word.compare(word.length() - suffix.length(), suffix.length(), suffix) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool ShiritoriGame::is_prefix_blacklisted(const std::string& prefix) const {
  return BLACKLIST_SUFFIXES.count(prefix) > 0;
}

bool ShiritoriGame::is_prefix_self_solving(const std::string& prefix) const {
  // A prefix is self-solving if there exists a word that starts with the prefix
  // and also ends with the prefix (creating a loop)
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

// Load database
bool ShiritoriGame::load_database(const std::string& dict_file, const std::string& patterns_file) {
  auto start_time = std::chrono::high_resolution_clock::now();

  std::cout << "[Loading database...]\n" << std::flush;

  dict.clear();
  rev_dict.clear();
  prefix_count_cache.clear();
  patterns.clear();

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
  int obscure_count = dict.size() * 3;
  int unique_prefixes = patterns.size();
  std::cout << "✓ Found " << obscure_count << " words with obscure suffixes\n";
  std::cout << "  (" << unique_prefixes << " unique obscure prefixes)\n" << std::flush;

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

  std::cout << "✓ Loaded " << dict.size() << " words and " << patterns.size() 
    << " patterns in " << duration.count() << "ms\n" << std::flush;

  return true;
}

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

bool ShiritoriGame::is_valid_word(const std::string& word) {
  std::string lower = word;
  to_lower_inplace(lower);
  return std::binary_search(dict.begin(), dict.end(), lower);
}

bool ShiritoriGame::is_used(const std::string& word) {
  std::string lower = word;
  to_lower_inplace(lower);
  return used_words.count(lower) > 0;
}

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

std::string ShiritoriGame::getCurrentPrefix() const {
  return current_prefix;
}

int ShiritoriGame::getCurrentDifficulty() const {
  return get_difficulty_level(turns_since_heart_loss);
}

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

std::string ShiritoriGame::find_valid_prefix(const std::string& word, int max_difficulty) const {
  for (int len = max_difficulty; len >= 1; --len) {
    std::string prefix = get_suffix(word, len);
    if (has_unused_words(prefix)) return prefix;
  }
  return "";
}

double calculateSolutionObscurityScore(const std::string& word) {
  double score = 0.0;

  // 1. WORD LENGTH - Longer words are more obscure
  if (word.length() >= 13) {
    score += (word.length() - 13) * 30.0 + 150.0;
  } else if (word.length() >= 11) {
    score += (word.length() - 11) * 25.0 + 100.0;
  } else if (word.length() >= 9) {
    score += (word.length() - 9) * 20.0 + 60.0;
  } else if (word.length() >= 7) {
    score += (word.length() - 7) * 15.0 + 30.0;
  } else {
    score += word.length() * 10.0;
  }

  // 2. RARE LETTERS
  const std::string very_rare = "jqxz";
  const std::string rare = "kvw";
  int very_rare_count = 0;
  int rare_count = 0;

  for (char c : word) {
    if (very_rare.find(c) != std::string::npos) {
      very_rare_count++;
    } else if (rare.find(c) != std::string::npos) {
      rare_count++;
    }
  }

  score += very_rare_count * 45.0;
  score += rare_count * 25.0;

  // 3. REPEATED LETTERS
  std::map<char, int> letter_counts;
  for (char c : word) {
    letter_counts[c]++;
  }

  for (const auto& pair : letter_counts) {
    if (pair.second >= 4) {
      score += 60.0;
    } else if (pair.second == 3) {
      score += 45.0;
    } else if (pair.second == 2) {
      score += 20.0;
    }
  }

  // 4. CONSECUTIVE DOUBLED LETTERS
  for (size_t i = 0; i < word.length() - 1; ++i) {
    if (word[i] == word[i+1]) {
      score += 30.0;
    }
  }

  // 5. CONSONANT CLUSTERS
  const std::string vowels = "aeiou";
  int consonant_streak = 0;
  int max_consonant_streak = 0;

  for (char c : word) {
    if (vowels.find(c) == std::string::npos) {
      consonant_streak++;
      max_consonant_streak = std::max(max_consonant_streak, consonant_streak);
    } else {
      consonant_streak = 0;
    }
  }

  if (max_consonant_streak >= 5) {
    score += max_consonant_streak * 25.0;
  } else if (max_consonant_streak >= 4) {
    score += max_consonant_streak * 20.0;
  } else if (max_consonant_streak >= 3) {
    score += max_consonant_streak * 15.0;
  }

  // 6. VOWEL CLUSTERS
  int vowel_streak = 0;
  int max_vowel_streak = 0;

  for (char c : word) {
    if (vowels.find(c) != std::string::npos) {
      vowel_streak++;
      max_vowel_streak = std::max(max_vowel_streak, vowel_streak);
    } else {
      vowel_streak = 0;
    }
  }

  if (max_vowel_streak >= 3) {
    score += max_vowel_streak * 20.0;
  }

  // 7. LETTER PATTERN REPETITION
  for (size_t len = 2; len <= 4; ++len) {
    if (word.length() < len) continue;

    std::map<std::string, int> substring_counts;
    for (size_t i = 0; i <= word.length() - len; ++i) {
      substring_counts[word.substr(i, len)]++;
    }
    for (const auto& pair : substring_counts) {
      if (pair.second >= 2) {
        score += pair.second * len * 10.0;
      }
    }
  }

  // 8. LOW LETTER DIVERSITY
  std::set<char> unique_letters(word.begin(), word.end());
  double diversity_ratio = (double)unique_letters.size() / word.length();
  if (diversity_ratio < 0.6) {
    score += 35.0;
  }

  return score;
}

// AI moves
std::vector<WordRank> ShiritoriGame::getTopAIMoves(const std::string& required_prefix, int top_n) {
  std::vector<WordRank> candidates;
  candidates.reserve(500);

  auto it = std::lower_bound(dict.begin(), dict.end(), required_prefix);

  int count = 0;
  const int MAX_CANDIDATES = 200;

  // Track which prefixes we've already used to ensure uniqueness
  std::unordered_set<std::string> used_prefixes;

  // STEP 1: Collect candidates with solution obscurity analysis
  while (it != dict.end() && it->rfind(required_prefix, 0) == 0 && count < MAX_CANDIDATES) {
    if (used_words.count(*it) == 0) {
      WordRank wr;
      wr.word = *it;

      // Find best creates-prefix
      auto prefix_info = find_best_prefix_static_cached_local(wr.word, prefix_count_cache, BLACKLIST_SUFFIXES, dict);
      wr.creates_prefix = prefix_info.first;
      wr.is_blacklisted = is_prefix_blacklisted(wr.creates_prefix);
      wr.is_self_solving = is_prefix_self_solving(wr.creates_prefix);

      // Skip bad candidates
      if (wr.is_blacklisted || wr.is_self_solving || word_ends_with_blacklisted_suffix(wr.word)) {
        ++it;
        continue;
      }

      // Skip if we've already used this prefix (ensure uniqueness)
      if (used_prefixes.count(wr.creates_prefix) > 0) {
        ++it;
        continue;
      }

      // Collect UNUSED solutions WITH OBSCURITY SCORES
      std::vector<std::pair<std::string, double>> solutions_with_scores;
      int solution_count = 0;
      double best_solution_obscurity = 0.0;
      int max_solution_length = 0;
      std::string best_solution_word = "";

      auto sol_it = std::lower_bound(dict.begin(), dict.end(), wr.creates_prefix);
      while (sol_it != dict.end() && sol_it->rfind(wr.creates_prefix, 0) == 0) {
        if (used_words.count(*sol_it) == 0) {
          double obscurity = calculateSolutionObscurityScore(*sol_it);
          solutions_with_scores.push_back({*sol_it, obscurity});
          solution_count++;

          if (obscurity > best_solution_obscurity) {
            best_solution_obscurity = obscurity;
            best_solution_word = *sol_it;
          }

          max_solution_length = std::max(max_solution_length, (int)sol_it->length());
        }
        ++sol_it;
      }

      // Skip if no solutions or already solved
      if (solution_count == 0 || solved_suffixes.count(wr.creates_prefix) > 0) {
        ++it;
        continue;
      }

      // Sort solutions by obscurity (most obscure first)
      std::sort(solutions_with_scores.begin(), solutions_with_scores.end(),
          [](const auto& a, const auto& b) {
          if (std::abs(a.second - b.second) > 0.001) return a.second > b.second;
          return a.first.length() > b.first.length();
          });

      wr.creates_prefix_solutions = solution_count;
      wr.max_solution_len = max_solution_length;

      // NEW RANKING SYSTEM (like solver.cpp):
      // PRIMARY: Fewer solutions (1 is best)
      // SECONDARY: Obscurity of best solution
      // TERTIARY: Length of best solution

      double total = 0.0;

      // PRIMARY: Solution count (fewer is better)
      total += 10000.0 / solution_count;

      // SECONDARY: Best solution obscurity
      total += best_solution_obscurity * 20.0;  // Higher weight for solution quality

      // TERTIARY: Longest solution length
      total += (max_solution_length - 5) * 25.0;

      // Small bonus for longer prefixes
      total += wr.creates_prefix.length() * 30.0;

      wr.total_score = total;
      wr.obscurity_score = best_solution_obscurity;  // Store for display
      wr.difficulty_level = (int)wr.creates_prefix.length();

      candidates.push_back(wr);
      used_prefixes.insert(wr.creates_prefix);  // Mark prefix as used
      count++;
    }
    ++it;
  }

  if (candidates.empty()) return {};

  // STEP 2: Sort by NEW ranking system
  std::sort(candidates.begin(), candidates.end(), [](const WordRank& a, const WordRank& b) {
      // PRIMARY: Fewer solutions
      if (a.creates_prefix_solutions != b.creates_prefix_solutions) {
      return a.creates_prefix_solutions < b.creates_prefix_solutions;
      }

      // SECONDARY: Higher solution obscurity
      if (std::abs(a.obscurity_score - b.obscurity_score) > 5.0) {
      return a.obscurity_score > b.obscurity_score;
      }

      // TERTIARY: Longer solutions
      if (a.max_solution_len != b.max_solution_len) {
      return a.max_solution_len > b.max_solution_len;
      }

      // Final tiebreaker
      return a.word < b.word;
      });

  // STEP 3: Return top N candidates (already unique by prefix)
  if (candidates.size() > static_cast<size_t>(top_n)) {
    candidates.resize(top_n);
  }

  return candidates;
}

void ShiritoriGame::processPlayerWord(const std::string& word) {
  std::string lower = word;
  to_lower_inplace(lower);

  word_chain.push_back(lower);
  used_words.insert(lower);
  ++turn_count;
  ++turns_since_heart_loss;

  solved_suffixes.insert(current_prefix);

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
  player_points = 0;
  turns_since_heart_loss = 0;
}

bool ShiritoriGame::wasTopSolve(const std::string& word) const {
  std::string lower = word;
  to_lower_inplace(const_cast<std::string&>(lower));
  return std::find(last_top_moves.begin(), last_top_moves.end(), lower) != last_top_moves.end();
}

std::string ShiritoriGame::getNewPrefix(const std::string& word, int difficulty) const {
  return find_valid_prefix(word, difficulty);
}

std::string ShiritoriGame::getAIMove() {
  if (word_chain.empty()) return "";

  const std::string& last_word = word_chain.back();
  int difficulty = get_difficulty_level(turns_since_heart_loss);

  std::string prefix = find_valid_prefix(last_word, difficulty);

  if (prefix.empty()) {
    if (dict.empty()) return "";

    std::uniform_int_distribution<> dis(0, std::min(1000, static_cast<int>(dict.size()) - 1));
    std::string word;
    int attempts = 0;
    do {
      word = dict[dis(rng)];
      ++attempts;
      if (attempts > 100) break;
    } while (used_words.count(word) > 0);

    if (used_words.count(word) > 0) return "";

    word_chain.push_back(word);
    used_words.insert(word);
    ++turn_count;
    ++turns_since_heart_loss;

    current_prefix = find_valid_prefix(word, get_difficulty_level(turns_since_heart_loss));

    auto top_moves_ranked = getTopAIMoves(current_prefix, 5);
    last_top_moves.clear();
    for (const auto& move : top_moves_ranked) {
      last_top_moves.push_back(move.word);
    }

    return word;
  }

  // STEP 1: Collect ALL unused words with the required prefix
  std::vector<WordRank> all_candidates;
  all_candidates.reserve(500);

  auto it = std::lower_bound(dict.begin(), dict.end(), prefix);

  while (it != dict.end() && it->rfind(prefix, 0) == 0) {
    if (used_words.count(*it) == 0) {
      WordRank wr;
      wr.word = *it;

      // Find best creates-prefix
      auto prefix_info = find_best_prefix_static_cached_local(wr.word, prefix_count_cache, BLACKLIST_SUFFIXES, dict);
      wr.creates_prefix = prefix_info.first;
      wr.is_blacklisted = is_prefix_blacklisted(wr.creates_prefix);
      wr.is_self_solving = is_prefix_self_solving(wr.creates_prefix);

      // Check if word itself is obscure
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

      // Collect UNUSED solutions
      std::vector<std::string> solutions;
      int solution_count = 0;
      int max_solution_length = 0;

      if (!wr.creates_prefix.empty()) {
        auto sol_it = std::lower_bound(dict.begin(), dict.end(), wr.creates_prefix);
        while (sol_it != dict.end() && sol_it->rfind(wr.creates_prefix, 0) == 0) {
          if (used_words.count(*sol_it) == 0) {
            solutions.push_back(*sol_it);
            solution_count++;
            max_solution_length = std::max(max_solution_length, (int)sol_it->length());
          }
          ++sol_it;
        }
      }

      wr.creates_prefix_solutions = solution_count;
      wr.max_solution_len = max_solution_length;

      // Calculate scores
      wr.obscurity_score = calculateObscurityScoreLocal(wr.creates_prefix, solution_count, solutions);

      double total = 0.0;

      // Penalize heavily if no solutions
      if (solution_count == 0) {
        total = -10000.0;
      } else {
        total += 10000.0 / solution_count;
        total += wr.obscurity_score * 15.0;
        total += (max_solution_length - 5) * 30.0;
        if (wr.is_obscure_word) {
          total += 1500.0 + wr.obscure_suffix_length * 100.0;
        }
        total += wr.creates_prefix.length() * 50.0;
      }

      // Penalize blacklisted/self-solving but don't exclude
      if (wr.is_blacklisted || wr.is_self_solving) {
        total -= 5000.0;
      }

      // Penalize if already solved
      if (solved_suffixes.count(wr.creates_prefix) > 0) {
        total -= 3000.0;
      }

      wr.total_score = total;
      wr.difficulty_level = (int)wr.creates_prefix.length();

      all_candidates.push_back(wr);
    }
    ++it;
  }

  if (all_candidates.empty()) return "";

  // STEP 2: Sort by total score (best first)
  std::sort(all_candidates.begin(), all_candidates.end(), [](const WordRank& a, const WordRank& b) {
      if (std::abs(a.total_score - b.total_score) > 0.001) return a.total_score > b.total_score;
      if (a.max_solution_len != b.max_solution_len) return a.max_solution_len > b.max_solution_len;
      return a.word < b.word;
      });

  // STEP 3: Try lookahead validation on top candidates
  std::vector<WordRank> viable_candidates;
  viable_candidates.reserve(all_candidates.size());

  // Try top 100 candidates with lookahead
  size_t check_limit = std::min(all_candidates.size(), static_cast<size_t>(100));

  for (size_t i = 0; i < check_limit; ++i) {
    const auto& candidate = all_candidates[i];

    // Skip if score is too negative (bad moves)
    if (candidate.total_score < -8000.0) continue;

    int player_difficulty = get_difficulty_level(turns_since_heart_loss + 1);
    std::string viable_player_prefix = find_valid_prefix(candidate.word, player_difficulty);

    if (viable_player_prefix.empty()) continue;

    // Temporarily mark as used to test player moves
    used_words.insert(candidate.word);
    bool ai_can_continue = false;

    auto player_it = std::lower_bound(dict.begin(), dict.end(), viable_player_prefix);

    // Check if any player response allows AI to continue
    int checked = 0;
    while (player_it != dict.end() && player_it->rfind(viable_player_prefix, 0) == 0 && checked < 50) {
      if (used_words.count(*player_it) == 0) {
        int ai_next_difficulty = get_difficulty_level(turns_since_heart_loss + 2);
        std::string ai_next_prefix = find_valid_prefix(*player_it, ai_next_difficulty);

        if (!ai_next_prefix.empty() && has_unused_words(ai_next_prefix)) {
          ai_can_continue = true;
          break;
        }
      }
      ++player_it;
      ++checked;
    }

    used_words.erase(candidate.word);

    if (ai_can_continue) {
      viable_candidates.push_back(candidate);
    }
  }

  // STEP 4: Fallback - if no viable candidates with lookahead, use best scored candidates anyway
  if (viable_candidates.empty()) {
    // Take top 20 candidates regardless of lookahead
    size_t fallback_size = std::min(all_candidates.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < fallback_size; ++i) {
      if (all_candidates[i].total_score > -8000.0) {
        viable_candidates.push_back(all_candidates[i]);
      }
    }
  }

  // If still no candidates, take anything available
  if (viable_candidates.empty() && !all_candidates.empty()) {
    viable_candidates.push_back(all_candidates[0]);
  }

  if (viable_candidates.empty()) return "";

  // STEP 5: Shuffle within difficulty tiers for variety
  size_t start_idx = 0;
  while (start_idx < viable_candidates.size()) {
    int current_count = viable_candidates[start_idx].creates_prefix_solutions;
    size_t end_idx = start_idx;

    while (end_idx < viable_candidates.size() &&
        viable_candidates[end_idx].creates_prefix_solutions == current_count) {
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

  // STEP 6: Select the best viable candidate
  std::string ai_word = viable_candidates[0].word;
  word_chain.push_back(ai_word);
  used_words.insert(ai_word);
  ++turn_count;
  ++turns_since_heart_loss;

  solved_suffixes.insert(prefix);

  current_prefix = find_valid_prefix(ai_word, get_difficulty_level(turns_since_heart_loss));

  auto top_moves_ranked = getTopAIMoves(current_prefix, 5);
  last_top_moves.clear();
  for (const auto& move : top_moves_ranked) {
    last_top_moves.push_back(move.word);
  }

  return ai_word;
}

std::vector<WordRank> ShiritoriGame::getRegularSolves(const std::string& required_prefix, int max_n) {
  std::vector<WordRank> candidates;
  candidates.reserve(max_n * 2);

  auto it = std::lower_bound(dict.begin(), dict.end(), required_prefix);

  int count = 0;

  // Collect ANY unused words with the prefix - minimal filtering
  while (it != dict.end() && it->rfind(required_prefix, 0) == 0) {
    if (used_words.count(*it) == 0) {
      WordRank wr;
      wr.word = *it;

      // Find what prefix this word creates (even if not ideal)
      std::string creates_prefix = "";
      int solution_count = 0;

      // Try to find ANY valid prefix this word creates
      for (int len = MAX_PREFIX_LEN; len >= 1; --len) {
        std::string potential_prefix = get_suffix(wr.word, len);

        // Count solutions for this potential prefix
        int temp_count = 0;
        auto sol_it = std::lower_bound(dict.begin(), dict.end(), potential_prefix);
        while (sol_it != dict.end() && sol_it->rfind(potential_prefix, 0) == 0) {
          if (used_words.count(*sol_it) == 0) {
            temp_count++;
          }
          ++sol_it;
        }

        // Take the first prefix that has ANY solutions
        if (temp_count > 0) {
          creates_prefix = potential_prefix;
          solution_count = temp_count;
          break;
        }
      }

      // If no prefix with solutions found, just use the longest suffix
      if (creates_prefix.empty()) {
        creates_prefix = get_suffix(wr.word, std::min(MAX_PREFIX_LEN, (int)wr.word.length()));
        solution_count = 0;
      }

      wr.creates_prefix = creates_prefix;
      wr.creates_prefix_solutions = solution_count;

      candidates.push_back(wr);
      count++;

      // Stop once we have enough
      if (candidates.size() >= static_cast<size_t>(max_n)) {
        break;
      }
    }
    ++it;
  }

  // Sort by solution count (ascending - fewer is harder, but we show them anyway)
  std::sort(candidates.begin(), candidates.end(), [](const WordRank& a, const WordRank& b) {
      // Prioritize words that create prefixes with solutions
      if (a.creates_prefix_solutions > 0 && b.creates_prefix_solutions == 0) return true;
      if (a.creates_prefix_solutions == 0 && b.creates_prefix_solutions > 0) return false;

      if (a.creates_prefix_solutions != b.creates_prefix_solutions) {
      return a.creates_prefix_solutions < b.creates_prefix_solutions;
      }
      return a.word < b.word;
      });

  // Return at most max_n
  if (candidates.size() > static_cast<size_t>(max_n)) {
    candidates.resize(max_n);
  }

  return candidates;
}
