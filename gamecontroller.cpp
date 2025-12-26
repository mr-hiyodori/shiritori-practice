#include "gamecontroller.h"
#include <QDebug>

GameController::GameController(QObject *parent)
    : QObject(parent)
    , m_game(nullptr)
    , m_difficulty(1)
{
    m_game = new ShiritoriGame();
    m_gameStatus = "Ready to load database";
}

GameController::~GameController()
{
    delete m_game;
}

bool GameController::loadDatabase(const QString& dictPath, const QString& patternsPath)
{
    if (!m_game) return false;
    
    qDebug() << "Loading database from:" << dictPath << "and" << patternsPath;
    
    bool success = m_game->load_database(dictPath.toStdString(), patternsPath.toStdString());
    
    if (success) {
        m_gameStatus = "Database loaded successfully! Ready to start.";
        emit gameStatusChanged();
        qDebug() << "Database loaded successfully";
    } else {
        m_gameStatus = "Failed to load database. Check file paths.";
        emit gameStatusChanged();
        qDebug() << "Failed to load database";
    }
    
    return success;
}

void GameController::startNewGame()
{
    if (!m_game) return;
    
    qDebug() << "Starting new game";
    
    m_game->reset_game();
    m_playerWords.clear();
    m_aiWords.clear();
    m_currentPrefix.clear();
    m_topSolves.clear();
    
    emit playerHeartsChanged();
    emit playerPointsChanged();
    emit playerWordsChanged();
    emit aiWordsChanged();
    emit currentPrefixChanged();
    emit topSolvesChanged();
    
    // AI makes first move
    std::string firstWord = m_game->getRandomStartWord();
    m_aiWords.append(QString::fromStdString(firstWord));
    emit aiWordsChanged();
    
    qDebug() << "AI first word:" << QString::fromStdString(firstWord);
    
    // Process AI turn to set up prefix for player
    processAITurn();
    
    m_gameStatus = "Game started! Your turn.";
    emit gameStatusChanged();
}

bool GameController::submitWord(const QString& word)
{
    if (!m_game) return false;
    
    std::string wordStr = word.toLower().toStdString();
    
    qDebug() << "Submitting word:" << word;
    
    // Validate word
    if (!m_game->is_valid_word(wordStr)) {
        qDebug() << "Word not in dictionary";
        emit wordInvalid("Word not in dictionary!");
        return false;
    }
    
    if (m_game->is_used(wordStr)) {
        qDebug() << "Word already used";
        emit wordInvalid("Word already used!");
        return false;
    }
    
    // Check if word starts with required prefix
    std::string requiredPrefix = m_currentPrefix.toStdString();
    if (wordStr.find(requiredPrefix) != 0) {
        qDebug() << "Word doesn't start with required prefix";
        emit wordInvalid("Word must start with: " + m_currentPrefix);
        return false;
    }
    
    // Check if it was a top solve BEFORE processing
    bool wasTopSolve = m_game->wasTopSolve(wordStr);
    
    // Word is valid, process it
    m_game->processPlayerWord(wordStr);
    m_playerWords.append(word.toLower());
    emit playerWordsChanged();
    
    qDebug() << "Word accepted, was top solve:" << wasTopSolve;
    
    // Notify if it was a top solve
    if (wasTopSolve) {
        emit topSolveAchieved();
    }
    
    emit playerHeartsChanged();
    emit playerPointsChanged();
    
    // Process AI turn
    processAITurn();
    
    return true;
}

void GameController::processAITurn()
{
    if (!m_game) return;
    
    qDebug() << "Processing AI turn";
    
    std::string aiWord = m_game->getAIMove();
    
    if (aiWord.empty()) {
        qDebug() << "AI has no valid move - player wins";
        emit gameOver(true); // Player wins
        return;
    }
    
    m_aiWords.append(QString::fromStdString(aiWord));
    emit aiWordsChanged();
    
    qDebug() << "AI played:" << QString::fromStdString(aiWord);
    
    // Update prefix for next player turn
    std::string newPrefix = m_game->getCurrentPrefix();
    
    if (newPrefix.empty()) {
        qDebug() << "No valid prefix - AI wins";
        emit gameOver(false); // AI wins
        return;
    }
    
    m_currentPrefix = QString::fromStdString(newPrefix);
    emit currentPrefixChanged();
    
    qDebug() << "New prefix for player:" << m_currentPrefix;
    
    m_difficulty = m_game->getCurrentDifficulty();
    emit difficultyChanged();
    
    updateTopSolves();
}

void GameController::updateTopSolves()
{
    if (!m_game) return;
    
    std::vector<std::string> topSolvesVec = m_game->getTopMoves(m_currentPrefix.toStdString());
    
    m_topSolves.clear();
    for (const auto& word : topSolvesVec) {
        m_topSolves.append(QString::fromStdString(word));
    }
    
    qDebug() << "Top solves updated:" << m_topSolves.size() << "words";
    
    emit topSolvesChanged();
}

void GameController::resetGame()
{
    if (!m_game) return;
    
    qDebug() << "Resetting game";
    
    m_game->reset_game();
    m_playerWords.clear();
    m_aiWords.clear();
    m_currentPrefix.clear();
    m_topSolves.clear();
    
    emit playerHeartsChanged();
    emit playerPointsChanged();
    emit playerWordsChanged();
    emit aiWordsChanged();
    emit currentPrefixChanged();
    emit topSolvesChanged();
    
    m_gameStatus = "Game reset";
    emit gameStatusChanged();
}

void GameController::endGame()
{
    qDebug() << "Game ended by player";
    emit gameOver(false); // Player surrendered, AI wins
}
