import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import Qt.labs.platform 1.1
import QtMultimedia 5.12

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1280
    height: 720
    minimumWidth: 1024
    minimumHeight: 600
    title: "Hiyodori's Shiritori"
    color: "#1f1f23"
    font.family: "Comic Neue"

    property string currentScreen: "mainMenu"
    property bool showTopSolveNotification: false
    property bool showHeartAnimation: false
    property bool showHeartLossAnimation: false
    property bool showInvalidWordAnimation: false
    property real timeRemaining: 12
    property real maxTime: 12
    property bool timerRunning: false
    property bool shouldShowTopSolves: false
    property int localPlayerHearts: 2
    property bool showWordChainDialog: false
    property bool showWordChoicesDialog: false
    property string chosenDictPath: ""
    property var wordChainModel: []
    property var wordChoicesSnapshots: []  // Change from var to specific type
    property bool showAlreadyUsedAnimation: false
    property bool showSolvesMode: false
    property var previousTopSolves: []

    // Sound Effects
    SoundEffect {
        id: typeSound
        source: "qrc:///Assets/type_sound.wav"
        volume: 0.25
    }
    
    SoundEffect {
        id: timeoutSound
        source: "qrc:///Assets/ran_out_of_time.wav"
        volume: 0.8
    }

    SoundEffect {
        id: submitSound
        source: "qrc:///Assets/submit.wav"
        volume: 0.5
    }

    SoundEffect {
        id: topSolveSound
        source: "qrc:///Assets/top_sound.wav"
        volume: 0.9
    }
    
    // Calculate max time based on difficulty
    function getMaxTimeForDifficulty(diff) {
        if (diff === 1) return 12
        if (diff === 2) return 10
        if (diff === 3) return 7
        return 5  // difficulty 4
    }

    // Update max time when difficulty changes
    onVisibleChanged: {
        if (visible && currentScreen === "gameplay") {
            maxTime = getMaxTimeForDifficulty(gameController.difficulty)
        }
    }

    // Word Chain dialog
    Rectangle {
        id: wordChainDialog
        visible: showWordChainDialog
        color: "transparent"
        radius: 12
        width: mainWindow.width * 0.92
        height: mainWindow.height * 0.92
        anchors.centerIn: parent
        border.width: 0
        z: 2000

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            // Title floating above the panel
            Text {
                text: "Word Chain"
                font.pixelSize: 54
                color: "white"
                Layout.alignment: Qt.AlignLeft
                font.family: "Comic Neue"
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Rectangle {
                    id: wcCloseBtn
                    width: 40
                    height: 40
                    radius: 20
                    color: "#e8e8e8"
                    border.color: "#00000033"
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: "#222"
                        font.pixelSize: 18
                    }

                    MouseArea { anchors.fill: parent; onClicked: showWordChainDialog = false }
                }
            }

            // Background panel that holds the chain grid
            Rectangle {
                id: wcPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#4a4d57"
                radius: 16

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 40
                    contentHeight: gridContainer.height
                    clip: true
                    
                    Grid {
                        id: gridContainer
                        width: parent.width
                        columns: 8
                        rowSpacing: 15
                        columnSpacing: (width - (columns * 115)) / (columns + 1)
                        leftPadding: (width - (columns * 115)) / (columns + 1)
                        
                        Repeater {
                            model: wordChainModel
                            
                            delegate: Item {
                                width: 115
                                height: 45
                                
                                property int itemIndex: index
                                
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 8
                                    // Index 0, 2, 4... = Player (blue), Index 1, 3, 5... = AI (red)
                                    color: (parent.itemIndex % 2 === 0) ? "#8fb7df" : "#d24a4a"

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                        font.family: "Comic Neue"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                        width: parent.width - 12
                                    }
                                }
                                
                                // Arrow overlay positioned to the right of this item
                                Text {
                                    anchors.left: parent.right
                                    anchors.leftMargin: (gridContainer.columnSpacing / 2) - 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "→"
                                    color: "white"
                                    font.pixelSize: 24
                                    font.bold: true
                                    visible: (parent.itemIndex + 1) % gridContainer.columns !== 0 && parent.itemIndex < wordChainModel.length - 1
                                    z: 100
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // When the dialog opens, snapshot the current full word chain
    onShowWordChainDialogChanged: {
        if (showWordChainDialog) {
            wordChainModel = gameController.getFullWordChain()
        }
    }

// Word Choices dialog (top solves history)
Rectangle {
    id: wordChoicesDialog
    visible: showWordChoicesDialog
    color: "transparent"
    radius: 12
    width: mainWindow.width * 0.85
    height: mainWindow.height * 0.85
    anchors.centerIn: parent
    border.width: 0
    z: 2000

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Title
        Text {
            text: "Word Choices"
            font.pixelSize: 54
            color: "white"
            Layout.alignment: Qt.AlignLeft
            font.family: "Comic Neue"
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Rectangle {
                id: wcChoicesCloseBtn
                width: 40
                height: 40
                radius: 20
                color: "#e8e8e8"
                border.color: "#00000033"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "#222"
                    font.pixelSize: 18
                }

                MouseArea { anchors.fill: parent; onClicked: showWordChoicesDialog = false }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#4a4d57"
            radius: 16

            // Show message if no data
            Text {
                visible: wordChoicesSnapshots.length === 0
                anchors.centerIn: parent
                text: "No word choices recorded yet.\nPlay some rounds first!"
                font.pixelSize: 24
                color: "#a0a0a0"
                horizontalAlignment: Text.AlignHCenter
                font.family: "Comic Neue"
            }

            Flickable {
                anchors.fill: parent
                anchors.margins: 40
                contentHeight: choicesColumn.height
                clip: true

                Column {
                    id: choicesColumn
                    width: parent.width
                    spacing: 20
                    
                    Repeater {
                        id: historyRepeater
                        model: wordChoicesSnapshots
                        
                        delegate: Column {
                            width: choicesColumn.width
                            spacing: 10
                            
                            // Show player word label
                            Text {
                                text: "Your word: " + (modelData.playerWord || "?")
                                font.pixelSize: 18
                                color: "#64c878"
                                font.bold: true
                                font.family: "Comic Neue"
                            }
                            
                            // Grid of top solves
                            Grid {
                                width: parent.width
                                columns: 3
                                columnSpacing: 20
                                rowSpacing: 20
                                
                                Repeater {
                                    model: modelData.topSolves || []
                                    
                                    delegate: Rectangle {
                                        width: (choicesColumn.width - 40) / 3
                                        height: 80
                                        radius: 12
                                        
                                        property bool isTopSolve: modelData.createsPrefixSolutions <= 5
                                        property bool isPlayerWord: parent.parent.parent.modelData.playerWord === modelData.word
                                        
                                        color: isTopSolve ? "#111217" : "#55565a"
                                        border.width: isPlayerWord ? 3 : 0
                                        border.color: isPlayerWord ? "#64c878" : "transparent"
                                        
                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            
                                            Text {
                                                text: modelData.word || "-"
                                                color: parent.parent.isTopSolve ? "#ffd166" : "#d3d3d3"
                                                font.pixelSize: 20
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                                font.family: "Comic Neue"
                                            }
                                            
                                            Text {
                                                text: "(" + (modelData.createsPrefixSolutions || 0) + " solutions)"
                                                color: parent.parent.isTopSolve ? "#ffd166" : "#a0a0a0"
                                                font.pixelSize: 14
                                                Layout.alignment: Qt.AlignHCenter
                                                font.family: "Comic Neue"
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Separator line
                            Rectangle {
                                width: parent.width
                                height: 2
                                color: "#3a3d47"
                                opacity: 0.5
                            }
                        }
                    }
                }
            }
        }
    }
}

// When the Word Choices dialog opens, snapshot the history from the controller
onShowWordChoicesDialogChanged: {
    if (showWordChoicesDialog) {
        console.log("=== WORD CHOICES DEBUG ===")
        var n = gameController.topSolvesHistorySize()
        console.log("History size:", n)
        
        // Clear and rebuild the array
        wordChoicesSnapshots = []
        
        for (var i = 0; i < n; ++i) {
            var playerWord = gameController.playerWordAtIndex(i)
            var topSolvesForTurn = gameController.topSolvesForIndex(i)
            
            console.log("Turn", i, "- Player word:", playerWord, "Top solves count:", topSolvesForTurn.length)
            
            wordChoicesSnapshots.push({
                playerWord: playerWord,
                topSolves: topSolvesForTurn
            })
        }
        
        console.log("Total snapshots created:", wordChoicesSnapshots.length)
        console.log("=========================")
    }
}

    // Connect to C++ signals
    Connections {
        target: gameController
        
        function onDifficultyChanged() {
            maxTime = getMaxTimeForDifficulty(gameController.difficulty)
            timeRemaining = maxTime
        }
        
        function onPlayerHeartsChanged() {
            localPlayerHearts = gameController.playerHearts
        }

        function onCurrentPrefixChanged() {
          shouldShowTopSolves = false
        }
        
        function onTopSolveAchieved() {
            showTopSolveNotification = true
            topSolveFloatTimer.start()
            
            // Check if we earned a heart (playerPoints resets to 0 after earning 6 points)
            if (gameController.playerPoints === 0) {
                showHeartAnimation = true
                heartAnimTimer.start()
            }
        }
        
        function onWordInvalid(reason) {
            showInvalidWordAnimation = true
            invalidWordTimer.start()

          // Show "already used" floating text if that's the reason
          if (reason === "Word already used!") {
            showAlreadyUsedAnimation = true
            alreadyUsedTimer.start()
          }
            // Don't stop timer - it keeps running
            userInput.text = ""
        }
        
        function onGameOver(playerWon) {
            gameOverMessage.text = playerWon ? "YOU WIN!" : "GAME OVER!"
            gameOverMessage.color = playerWon ? "#64c878" : "#ff6b6b"
            timerRunning = false
            currentScreen = "gameOver"
        }
    }

    // Top solve notification timer
    Timer {
        id: topSolveFloatTimer
        interval: 2000
        onTriggered: showTopSolveNotification = false
    }

    // Invalid word timer
    Timer {
        id: invalidWordTimer
        interval: 600
        onTriggered: showInvalidWordAnimation = false
    }

    // Heart animation timer
    Timer {
        id: heartAnimTimer
        interval: 2000
        onTriggered: showHeartAnimation = false
    }

    // Heart loss animation timer
    Timer {
        id: heartLossTimer
        interval: 2000
        onTriggered: showHeartLossAnimation = false
    }

    // Game timer
    Timer {
        id: gameTimer
        interval: 100
        repeat: true
        running: timerRunning
        onTriggered: {
          timeRemaining -= 0.1
          if (timeRemaining <= 0) {
            timerRunning = false

            // Play timeout sound
            timeoutSound.play()

            showHeartLossAnimation = true
            heartLossTimer.start()
            userInput.text = ""

                // Inform backend of heart loss; backend will decrement and reset points
                gameController.onHeartLoss()

                // Update local view of hearts from controller
                localPlayerHearts = gameController.playerHearts

                // Check if game over
                if (localPlayerHearts <= 0) {
                    gameOverMessage.text = "GAME OVER!"
                    gameOverMessage.color = "#ff6b6b"
                    currentScreen = "gameOver"
                } else {
                    // Continue game - reset timer and get new prefix
                    maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                    timeRemaining = maxTime
                    timerRunning = true
                }
            }
        }
    }

    // Already used animation timer
    Timer {
      id: alreadyUsedTimer
      interval: 2000
      onTriggered: showAlreadyUsedAnimation = false
    }

    // Top solves display (window-level overlay, only visible during gameplay)
    Rectangle {
      id: topSolvesOverlay
      x: mainWindow.width / 2 - 300
      y: 170
      width: 600
      height: 35
      color: "transparent"
      radius: 8
      border.color: "transparent"
      border.width: 1
      // Show Solves mode: always show current prefix solves
      // No Show Solves mode: only show previous prefix solves after submission
      visible: currentScreen === "gameplay" && 
      ((showSolvesMode && gameController.topSolves.length > 0) ||
      (!showSolvesMode && shouldShowTopSolves && previousTopSolves.length > 0))
      z: 1000

      Text {
        id: topSolvesOverlayText
        anchors.centerIn: parent
        text: {
          // In Show Solves mode: display current solves
          // In No Show Solves mode: display previous solves
          var displaySolves = showSolvesMode ? gameController.topSolves : previousTopSolves

          if (displaySolves.length === 0) return ""
          var solves = ""
          for (var i = 0; i < Math.min(4, displaySolves.length); i++) {
            if (i > 0) solves += ", "
            var move = displaySolves[i]
            if (move && typeof move === 'object') {
              solves += move.word + " (" + move.createsPrefixSolutions + ")"
            } else {
              solves += move
            }
          }
          return solves
        }
        font.pixelSize: 22
        color: "#6495ed"
        font.family: "Comic Neue"
      }
    }

    // Regular solves display (below top solves)
    Rectangle {
      id: regularSolvesOverlay
      x: mainWindow.width / 2 - 300
      y: 210
      width: 600
      height: 35
      color: "transparent"
      radius: 8
      border.color: "transparent"
      border.width: 1
      visible: currentScreen === "gameplay" && showSolvesMode && gameController.regularSolves.length > 0
      z: 999

      Text {
        id: regularSolvesOverlayText
        anchors.centerIn: parent
        text: {
          if (gameController.regularSolves.length === 0) return ""
          // Only show the first word
          var move = gameController.regularSolves[0]
          if (move && typeof move === 'object') {
            return move.word + " (" + move.createsPrefixSolutions + ")"
          }
          return move
        }
        font.pixelSize: 20
        color: "#7a7a7a"
        font.family: "Comic Neue"
      }
    }
    // File dialog & drop area for loading custom dictionary files
    FileDialog {
        id: fileDialog
        title: "Select dictionary file"
        nameFilters: [ "Text files (*.txt)", "All files (*)" ]
        onAccepted: {
            if (fileDialog.fileUrls.length > 0) {
                var f = fileDialog.fileUrls[0].toLocalFile()
                chosenDictPath = f
                // attempt to load; patterns path left empty
                var ok = gameController.loadDatabase(f, "")
                if (ok) console.log("Loaded dict:", f)
                else console.log("Failed to load dict:", f)
            }
        }
    }

    StackLayout {
      anchors.fill: parent
      currentIndex: {
        switch(currentScreen) {
          case "mainMenu": return 0;
          case "gameplay": return 1;
          case "gameOver": return 2;
          default: return 0;
        }
      }

      // ========== Screen 1: Main Menu ==========
      Item {
        Rectangle {
          anchors.fill: parent
          color: "#1f1f23"

          ColumnLayout {
            anchors.centerIn: parent
            spacing: 40

            Text {
              text: "HIYODORI'S SHIRITORI"
              font.pixelSize: 58
              font.bold: true
              color: "white"
              Layout.alignment: Qt.AlignHCenter
              font.family: "Comic Neue"
            }

            Rectangle {
              Layout.preferredWidth: 750
              Layout.preferredHeight: 100
              color: "#4a4d57"
              radius: 12

              Text {
                anchors.centerIn: parent
                text: "Which gameplay would you like?"
                font.pixelSize: 32
                color: "#f5c542"
                font.bold: true
                font.family: "Comic Neue"
              }
            }

            RowLayout {
              spacing: 40
              Layout.alignment: Qt.AlignHCenter

              // Show Solves mode button
              Button {
                Layout.preferredWidth: 360
                Layout.preferredHeight: 70
                background: Rectangle {
                  color: parent.hovered ? "#181818" : "#64c878"
                  radius: 24
                }
                contentItem: Text {
                  anchors.fill: parent
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  text: "Show Solves"
                  font.pixelSize: 28
                  color: "white"
                  font.family: "Comic Neue"
                }
                onClicked: {
                  shouldShowTopSolves = false
                  previousTopSolves = []
                  showSolvesMode = true

                  // Reset game state
                  localPlayerHearts = 2
                  timeRemaining = 12
                  timerRunning = false
                  showTopSolveNotification = false
                  showHeartAnimation = false
                  showHeartLossAnimation = false
                  showInvalidWordAnimation = false
                  showAlreadyUsedAnimation = false
                  showWordChainDialog = false
                  showWordChoicesDialog = false

                  var dictPath = "./Dictionary/last_letter.txt"
                  var patternPath = "./Dictionary/rare_prefix.txt"
                  chosenDictPath = dictPath

                  if (gameController.loadDatabase(dictPath, patternPath)) {
                    gameController.startNewGame()
                    currentScreen = "gameplay"
                    localPlayerHearts = gameController.playerHearts
                    maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                    timeRemaining = maxTime
                    timerRunning = true
                    Qt.callLater(function() { userInput.forceActiveFocus() })
                  } else {
                    console.log("Failed to load default dict")
                  }
                }
              }

              // No Show Solves mode button
              Button {
                Layout.preferredWidth: 360
                Layout.preferredHeight: 70
                background: Rectangle {
                  color: parent.hovered ? "#181818" : "#ff6b6b"
                  radius: 24
                }
                contentItem: Text {
                  anchors.fill: parent
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  text: "No Show Solves"
                  font.pixelSize: 24
                  color: "white"
                  font.family: "Comic Neue"
                }
                onClicked: {
                  shouldShowTopSolves = false
                  previousTopSolves = []
                  showSolvesMode = false

                  // Reset game state
                  localPlayerHearts = 2
                  timeRemaining = 12
                  timerRunning = false
                  showTopSolveNotification = false
                  showHeartAnimation = false
                  showHeartLossAnimation = false
                  showInvalidWordAnimation = false
                  showAlreadyUsedAnimation = false
                  showWordChainDialog = false
                  showWordChoicesDialog = false

                  var dictPath = "./Dictionary/last_letter.txt"
                  var patternPath = "./Dictionary/rare_prefix.txt"
                  chosenDictPath = dictPath

                  if (gameController.loadDatabase(dictPath, patternPath)) {
                    gameController.startNewGame()
                    currentScreen = "gameplay"
                    localPlayerHearts = gameController.playerHearts
                    maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                    timeRemaining = maxTime
                    timerRunning = true
                    Qt.callLater(function() { userInput.forceActiveFocus() })
                  } else {
                    console.log("Failed to load default dict")
                  }
                }
              }
          }
        }
      }
    }

      // ========== Screen 2: Gameplay ==========
      Item {
        onVisibleChanged: {
          if (visible) {
            userInput.forceActiveFocus()
          }
        }

        Rectangle {
          anchors.fill: parent
          color: "#1f1f23"

          ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 20

            // Top section: Input with prefix (centered)
            Item {
              Layout.fillWidth: true
              Layout.preferredHeight: 150

              Rectangle {
                id: inputBox
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -10
                width: 600
                height: 80
                color: "#2b2d31"
                radius: 12

                // Shake animation for invalid word
                SequentialAnimation {
                  id: shakeAnimation
                  running: showInvalidWordAnimation
                  NumberAnimation { target: inputBox; property: "x"; to: inputBox.x - 10; duration: 50 }
                  NumberAnimation { target: inputBox; property: "x"; to: inputBox.x + 10; duration: 50 }
                  NumberAnimation { target: inputBox; property: "x"; to: inputBox.x - 10; duration: 50 }
                  NumberAnimation { target: inputBox; property: "x"; to: inputBox.x + 10; duration: 50 }
                  NumberAnimation { target: inputBox; property: "x"; to: inputBox.x; duration: 50 }
                }

                // Red flash for invalid word
                ColorAnimation on color {
                  running: showInvalidWordAnimation
                  from: "#ff6b6b"
                  to: "#2b2d31"
                  duration: 600
                }

                RowLayout {
                  anchors.fill: parent
                  anchors.margins: 10
                  spacing: 20

                  Text {
                    text: "Difficulty: " + gameController.difficulty + "/4"
                    font.pixelSize: 16
                    color: "#a0a0a0"
                    Layout.alignment: Qt.AlignVCenter
                    font.family: "Comic Neue"
                  }

                  Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: "#1a1c1f"
                    radius: 8

                    Row {
                      anchors.centerIn: parent
                      spacing: 0

                      Text {
                        text: gameController.currentPrefix
                        font.pixelSize: 36
                        font.bold: true
                        color: "#f5c542"
                        font.family: "Comic Neue"
                      }

                      TextInput {
                        id: userInput
                        font.pixelSize: 36
                        font.bold: true
                        color: "white"
                        width: 300
                        clip: true
                        activeFocusOnPress: true
                        font.family: "Comic Neue"

                        // Play typing sound with progressive volume
                        onTextChanged: {
                          if (text.length > 0) {
                            var volumeMultiplier = Math.min(text.length / 10.0, 1.0)
                            typeSound.volume = 0.25 + (0.25 * volumeMultiplier)
                            typeSound.play()
                          }
                        }

                        Text {
                          visible: parent.contentWidth > 300
                          text: "..."
                          font: parent.font
                          color: parent.color
                          x: -20
                          anchors.verticalCenter: parent.verticalCenter
                        }

                        Component.onCompleted: {
                          forceActiveFocus()
                        }

                        Keys.onReturnPressed: {
                          var fullWord = gameController.currentPrefix + text
                          if (fullWord.length > 0) {
                            var isTopSolve = false
                            for (var i = 0; i < gameController.topSolves.length; i++) {
                              var topSolveWord = gameController.topSolves[i].word
                              if (topSolveWord === fullWord.toLowerCase()) {
                                isTopSolve = true
                                break
                              }
                            }

                            if (!showSolvesMode) {
                              previousTopSolves = []
                              for (var j = 0; j < gameController.topSolves.length; j++) {
                                previousTopSolves.push(gameController.topSolves[j])
                              }
                            }

                            var success = gameController.submitWord(fullWord)
                            if (success) {
                              if (isTopSolve) {
                                topSolveSound.play()
                              } else {
                                submitSound.play()
                              }

                              // In No Show Solves mode: enable display of the saved previous solves
                              if (!showSolvesMode) {
                                shouldShowTopSolves = true
                              }

                              text = ""
                              maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                              timeRemaining = maxTime
                              timerRunning = true
                              forceActiveFocus()
                            }
                          }
                        }
                      }
                    }
                  }
                }

                // Timer progress bar below the input box
                Rectangle {
                  anchors.bottom: parent.bottom
                  width: parent.width
                  height: 4
                  radius: 2

                  Rectangle {
                    width: parent.width * (timeRemaining / maxTime)
                    height: parent.height
                    color: timeRemaining > (maxTime * 0.3) ? "#5cb574" : "#ff6b6b"
                    radius: 2

                    Behavior on width {
                      NumberAnimation { duration: 100 }
                    }
                  }
                }
              }
            }

            // Main gameplay area - centered avatars with words below
            Item {
              Layout.fillWidth: true
              Layout.fillHeight: true

              RowLayout {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -40
                spacing: 80

                // Player side
                ColumnLayout {
                  Layout.alignment: Qt.AlignTop
                  spacing: 10

                  // Hearts display on top of player (closer to avatar)
                  Item {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter

                    RowLayout {
                      anchors.centerIn: parent
                      spacing: 10

                      Repeater {
                        model: Math.max(2, localPlayerHearts)
                        delegate: Rectangle {
                          width: 40
                          height: 40
                          color: "transparent"
                          radius: 20

                          Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            source: "qrc:///Assets/heart.png"
                            opacity: index < localPlayerHearts ? 1.0 : 0.4
                          }
                        }
                      }
                    }

                    // Floating +1 Heart animation
                    Text {
                      id: heartFloatText
                      text: "+1 Heart!"
                      font.pixelSize: 20
                      font.bold: true
                      color: "#64c878"
                      anchors.horizontalCenter: parent.horizontalCenter
                      y: 25
                      visible: showHeartAnimation
                      font.family: "Comic Neue"

                      NumberAnimation on y {
                        running: showHeartAnimation
                        from: 25
                        to: -20
                        duration: 2000
                      }

                      NumberAnimation on opacity {
                        running: showHeartAnimation
                        from: 1.0
                        to: 0.0
                        duration: 2000
                      }
                    }

                    // Floating -1 Heart animation
                    Text {
                      id: heartLossText
                      text: "-1 Heart!"
                      font.pixelSize: 20
                      font.bold: true
                      color: "#ff6b6b"
                      anchors.horizontalCenter: parent.horizontalCenter
                      y: 25
                      visible: showHeartLossAnimation
                      font.family: "Comic Neue"

                      NumberAnimation on y {
                        running: showHeartLossAnimation
                        from: 25
                        to: -20
                        duration: 2000
                      }

                      NumberAnimation on opacity {
                        running: showHeartLossAnimation
                        from: 1.0
                        to: 0.0
                        duration: 2000
                      }
                    }
                  }

                  // Player avatar
                  Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 150
                    Layout.alignment: Qt.AlignHCenter
                    color: "#6b6e76"
                    radius: 24
                    clip: true
                    border.width: 2
                    border.color: "#483d8b"

                    Image {
                      anchors.fill: parent
                      source: "qrc:///Assets/player_avatar.jpg"
                      fillMode: Image.PreserveAspectCrop
                    }
                  }

                  // Player's last submitted word overlay
                  Item {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 35
                    Layout.alignment: Qt.AlignHCenter

                    Rectangle {
                      visible: gameController.playerWords.length > 0
                      anchors.centerIn: parent
                      width: Math.min(playerWordText.implicitWidth + 20, 220)
                      height: 40
                      color: "transparent"
                      radius: 8

                      Text {
                        id: playerWordText
                        anchors.centerIn: parent
                        font.pixelSize: 28
                        font.bold: true
                        textFormat: Text.RichText
                        horizontalAlignment: Text.AlignHCenter
                        maximumLineCount: 1
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, 200)
                        font.family: "Comic Neue"
                        text: {
                          if (gameController.playerWords.length === 0) return ""
                          var word = gameController.playerWords[gameController.playerWords.length - 1]
                          var prefix = ""
                          for (var i = Math.min(4, word.length); i >= 1; i--) {
                            prefix = word.substring(0, i)
                            break
                          }
                          var suffix = word.substring(prefix.length)
                          return '<span style="color: #f5c542;">' + prefix + '</span><span style="color: white;">' + suffix + '</span>'
                        }
                      }
                    }
                  }
                }

                // Star progress display (3x3 grid between player and AI)
                Item {
                  Layout.preferredWidth: 200
                  Layout.preferredHeight: 200
                  Layout.alignment: Qt.AlignVCenter

                  Column {
                    anchors.centerIn: parent
                    spacing: 10

                    // Top row - 3 stars (indices 0, 1, 2)
                    Row {
                      spacing: 15
                      anchors.horizontalCenter: parent.horizontalCenter

                      Repeater {
                        model: 3
                        delegate: Text {
                          text: "★"
                          font.pixelSize: 42
                          color: index < gameController.playerPoints ? "#f5c542" : "#5a5d64"

                          Behavior on color {
                            ColorAnimation { duration: 300 }
                          }
                        }
                      }
                    }

                    // Middle row - 3 stars (indices 3, 4, 5)
                    Row {
                      spacing: 15
                      anchors.horizontalCenter: parent.horizontalCenter

                      Repeater {
                        model: 3
                        delegate: Text {
                          text: "★"
                          font.pixelSize: 42
                          color: (index + 3) < gameController.playerPoints ? "#f5c542" : "#5a5d64"

                          Behavior on color {
                            ColorAnimation { duration: 300 }
                          }
                        }
                      }
                    }

                    // Bottom row - 3 stars (indices 6, 7, 8)
                    Row {
                      spacing: 15
                      anchors.horizontalCenter: parent.horizontalCenter

                      Repeater {
                        model: 3
                        delegate: Text {
                          text: "★"
                          font.pixelSize: 42
                          color: (index + 6) < gameController.playerPoints ? "#f5c542" : "#5a5d64"

                          Behavior on color {
                            ColorAnimation { duration: 300 }
                          }
                        }
                      }
                    }
                  }

                  // Floating "TOP SOLVE!" text
                  Text {
                    id: topSolveFloatText
                    text: "TOP SOLVE!"
                    font.pixelSize: 28
                    font.bold: true
                    color: "#00d9ff"
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 230
                    visible: showTopSolveNotification

                    NumberAnimation on y {
                      running: showTopSolveNotification
                      from: 230
                      to: 170
                      duration: 2000
                    }

                    NumberAnimation on opacity {
                      running: showTopSolveNotification
                      from: 1.0
                      to: 0.0
                      duration: 2000
                    }
                  }

                  // Floating "ALREADY USED!" text
                  Text {
                    id: alreadyUsedFloatText
                    text: "ALREADY USED!"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#ff6b6b"
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 230
                    visible: showAlreadyUsedAnimation
                    font.family: "Comic Neue"

                    NumberAnimation on y {
                      running: showAlreadyUsedAnimation
                      from: 230
                      to: 170
                      duration: 2000
                    }

                    NumberAnimation on opacity {
                      running: showAlreadyUsedAnimation
                      from: 1.0
                      to: 0.0
                      duration: 2000
                    }
                  }
                }

                // AI side
                ColumnLayout {
                  Layout.alignment: Qt.AlignTop
                  spacing: 10

                  // AI name (to match hearts on player side)
                  Item {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                      anchors.centerIn: parent
                      text: "Miss Justice"
                      font.pixelSize: 28
                      font.bold: true
                      color: "white"
                      font.family: "Comic Neue"
                    }
                  }

                  // AI avatar
                  Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 150
                    Layout.alignment: Qt.AlignHCenter
                    color: "#6b6e76"
                    radius: 24
                    clip: true
                    border.width: 2
                    border.color: "#483d8b"

                    Image {
                      anchors.fill: parent
                      source: "qrc:///Assets/AI_avatar.png"
                      fillMode: Image.PreserveAspectCrop
                    }
                  }

                  // AI's last submitted word overlay
                  Item {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 35
                    Layout.alignment: Qt.AlignHCenter

                    Rectangle {
                      visible: gameController.aiWords.length > 0
                      anchors.centerIn: parent
                      width: Math.min(aiWordText.implicitWidth + 20, 220)
                      height: 40
                      color: "transparent"
                      radius: 8

                      Text {
                        id: aiWordText
                        anchors.centerIn: parent
                        text: gameController.aiWords.length > 0 ? gameController.aiWords[gameController.aiWords.length - 1] : ""
                        font.pixelSize: 28
                        font.bold: true
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        maximumLineCount: 1
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, 200)
                        font.family: "Comic Neue"
                      }
                    }
                  }
                }
              }
            }

            // Bottom button
            Button {
              Layout.alignment: Qt.AlignHCenter
              Layout.preferredWidth: 150
              Layout.preferredHeight: 50

              background: Rectangle {
                color: parent.hovered ? "#d55555" : "#ff6b6b"
                radius: 8
              }

              contentItem: Text {
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "Quit"
                color: "white"
                font.pixelSize: 18
                font.bold: true
              }

              onClicked: {
                timerRunning = false
                gameController.endGame()
              }
            }
          }
        }
      }

      // ========== Screen 3: Game Over ==========
      Item {
        Rectangle {
          anchors.fill: parent
          color: "#1f1f23"

          ColumnLayout {
            anchors.centerIn: parent
            spacing: 50

            Text {
              id: gameOverMessage
              text: "GAME OVER!"
              font.pixelSize: 80
              font.bold: true
              color: "#ff6b6b"
              Layout.alignment: Qt.AlignHCenter
              font.family: "Arial Black"
            }

            RowLayout {
              Layout.alignment: Qt.AlignHCenter
              spacing: 60

              // Word Chain button with icon
              Button {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 120
                onClicked: showWordChainDialog = true
                background: Rectangle { color: "transparent" }
                Column {
                  anchors.fill: parent
                  anchors.margins: 8
                  spacing: 6
                  Image { source: "qrc:///Assets/word_chain.png"; anchors.horizontalCenter: parent.horizontalCenter; width: 80; height: 80; fillMode: Image.PreserveAspectFit }
                  Text { text: "Word Chain"; font.pixelSize: 20; color: "white"; anchors.horizontalCenter: parent.horizontalCenter }
                }
              }

              // Word Choices button with icon
              Button {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 120
                onClicked: showWordChoicesDialog = true
                background: Rectangle { color: "transparent" }
                Column {
                  anchors.fill: parent
                  anchors.margins: 8
                  spacing: 6
                  Image { source: "qrc:///Assets/word_choice.png"; anchors.horizontalCenter: parent.horizontalCenter; width: 80; height: 80; fillMode: Image.PreserveAspectFit }
                  Text { text: "Word Choices"; font.pixelSize: 20; color: "white"; anchors.horizontalCenter: parent.horizontalCenter }
                }
              }
            }

            Rectangle {
              Layout.preferredWidth: 650
              Layout.preferredHeight: 90
              color: "#4a4d57"
              radius: 12

              Text {
                anchors.centerIn: parent
                text: "Do you wish to play again?"
                font.pixelSize: 32
                font.bold: true
                color: "#f5c542"
                font.family: "Comic Neue"
              }
            }

            RowLayout {
              Layout.alignment: Qt.AlignHCenter
              spacing: 40

              Button {
                Layout.preferredWidth: 240
                Layout.preferredHeight: 70

                background: Rectangle {
                  color: parent.hovered ? "#181818": "white"
                  radius: 12
                }

                contentItem: Text {
                  anchors.fill: parent
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  text: "Yes"
                  font.pixelSize: 32
                  font.bold: true
                  color: "#2b2d31"
                  font.family: "Comic Neue"
                }

                onClicked: {
                  currentScreen = "mainMenu"
                }
              }

              Button {
                Layout.preferredWidth: 240
                Layout.preferredHeight: 70

                background: Rectangle {
                  color: parent.hovered ? "#181818" : "#ff6b6b"
                  radius: 12
                }

                contentItem: Text {
                  anchors.fill: parent
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  text: "No"
                  font.pixelSize: 32
                  font.bold: true
                  color: "white"
                  font.family: "Comic Neue"
                }

                onClicked: Qt.quit()
              }
            }
          }
        }
      }
    }
  }
