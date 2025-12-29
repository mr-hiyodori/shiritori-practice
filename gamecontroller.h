#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include "shiritorigame.h"
#include <QList>
#include <vector>

class GameController : public QObject
{
    Q_OBJECT
    
    // Properties accessible from QML
    Q_PROPERTY(int playerHearts READ playerHearts NOTIFY playerHeartsChanged)
    Q_PROPERTY(int playerPoints READ playerPoints NOTIFY playerPointsChanged)
    Q_PROPERTY(QString currentPrefix READ currentPrefix NOTIFY currentPrefixChanged)
    Q_PROPERTY(QVariantList topSolves READ topSolves NOTIFY topSolvesChanged)
    Q_PROPERTY(QVariantList regularSolves READ regularSolves NOTIFY regularSolvesChanged)
    Q_PROPERTY(QStringList playerWords READ playerWords NOTIFY playerWordsChanged)
    Q_PROPERTY(QStringList aiWords READ aiWords NOTIFY aiWordsChanged)
    Q_PROPERTY(QString gameStatus READ gameStatus NOTIFY gameStatusChanged)
    Q_PROPERTY(int difficulty READ difficulty NOTIFY difficultyChanged)

public:
    explicit GameController(QObject *parent = nullptr);
    ~GameController();

    // Property getters
    int playerHearts() const { return m_game ? m_game->getPlayerHearts() : 2; }
    int playerPoints() const { return m_game ? m_game->getPlayerPoints() : 0; }
    QString currentPrefix() const { return m_currentPrefix; }
    QVariantList topSolves() const { return m_topSolves; }
    QVariantList regularSolves() const { return m_regularSolves; }
    QStringList playerWords() const { return m_playerWords; }
    QStringList aiWords() const { return m_aiWords; }
    QString gameStatus() const { return m_gameStatus; }
    int difficulty() const { return m_difficulty; }

    // Invokable methods (callable from QML)
    Q_INVOKABLE bool loadDatabase(const QString& dictPath, const QString& patternsPath);
    Q_INVOKABLE void startNewGame();
    Q_INVOKABLE bool submitWord(const QString& word);
    Q_INVOKABLE void resetGame();
    Q_INVOKABLE void endGame();
    Q_INVOKABLE void onHeartLoss();
    Q_INVOKABLE QStringList getFullWordChain();
    Q_INVOKABLE int topSolvesHistorySize() const;
    Q_INVOKABLE QVariantList topSolvesForIndex(int idx) const;
    Q_INVOKABLE QString playerWordAtIndex(int index) const;

signals:
    // Signals to notify QML of changes
    void playerHeartsChanged();
    void playerPointsChanged();
    void currentPrefixChanged();
    void topSolvesChanged();
    void regularSolvesChanged();
    void playerWordsChanged();
    void aiWordsChanged();
    void gameStatusChanged();
    void difficultyChanged();
    void wordInvalid(const QString& reason);
    void topSolveAchieved();
    void gameOver(bool playerWon);

private:
    ShiritoriGame* m_game;
    QString m_currentPrefix;
    QVariantList m_topSolves;
    QVariantList m_regularSolves;
    QList<QVariantList> m_topSolvesHistory;
    std::vector<std::string> m_playerWordHistory;
    QStringList m_playerWords;
    QStringList m_aiWords;
    QString m_gameStatus;
    int m_difficulty;

    void updateTopSolves();
    void processAITurn();
};

#endif // GAMECONTROLLER_H
