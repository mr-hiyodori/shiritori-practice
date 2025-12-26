import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1280
    height: 720
    minimumWidth: 1024
    minimumHeight: 600
    title: "Hiyodori's Shiritori"
    color: "#2b2d31"

    property string currentScreen: "mainMenu"
    property bool showTopSolveNotification: false
    property bool showHeartAnimation: false
    property bool showHeartLossAnimation: false
    property bool showInvalidWordAnimation: false
    property real timeRemaining: 12
    property real maxTime: 12
    property bool timerRunning: false
    property bool hasSubmittedOnce: false
    property int localPlayerHearts: 2

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
                showHeartLossAnimation = true
                heartLossTimer.start()
                userInput.text = ""
                
                // Decrease heart locally
                localPlayerHearts = Math.max(0, localPlayerHearts - 1)
                
                // Check if game over
                if (localPlayerHearts <= 0) {
                    gameOverMessage.text = "GAME OVER!"
                    gameOverMessage.color = "#ff6b6b"
                    currentScreen = "gameOver"
                } else {
                    // Continue game - reset timer
                    maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                    timeRemaining = maxTime
                    timerRunning = true
                }
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
                color: "#2b2d31"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 40

                    Text {
                        text: "HIYODORI'S SHIRITORI"
                        font.pixelSize: 58
                        font.bold: true
                        color: "white"
                        Layout.alignment: Qt.AlignHCenter
                        font.family: "Arial Black"
                    }

                    Rectangle {
                        Layout.preferredWidth: 750
                        Layout.preferredHeight: 100
                        color: "#4a4d57"
                        radius: 12

                        Text {
                            anchors.centerIn: parent
                            text: "Ready to play?"
                            font.pixelSize: 28
                            color: "#f5c542"
                            font.bold: true
                        }
                    }

                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 280
                        Layout.preferredHeight: 80
                        
                        background: Rectangle {
                            color: parent.hovered ? "#6b6e76" : "#5a5d64"
                            radius: 12
                        }

                        contentItem: Text {
                            text: "Start Game"
                            font.pixelSize: 36
                            font.bold: true
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            var dictPath = "/home/hiyodori/Words/Dictionaries/Last_Letter/last_letter.txt"
                            var patternPath = "/home/hiyodori/Words/Dictionaries/Last_Letter/rare_prefix.txt"
                            
                            if (gameController.loadDatabase(dictPath, patternPath)) {
                                gameController.startNewGame()
                                currentScreen = "gameplay"
                                hasSubmittedOnce = false
                                localPlayerHearts = gameController.playerHearts
                                maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                                timeRemaining = maxTime
                                timerRunning = true
                                Qt.callLater(function() {
                                    userInput.forceActiveFocus()
                                })
                            } else {
                                console.log("Failed to load dictionary files")
                            }
                        }
                    }

                    Text {
                        text: "Dictionary: /home/hiyodori/Words/Dictionaries/Last_Letter/"
                        font.pixelSize: 14
                        color: "#808080"
                        Layout.alignment: Qt.AlignHCenter
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
                color: "#2b2d31"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 30
                    spacing: 20

                    // Top section: Input with prefix (centered)
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        
                        Rectangle {
                            id: inputBox
                            anchors.horizontalCenter: parent.horizontalCenter
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
                                        }

                                        TextInput {
                                            id: userInput
                                            font.pixelSize: 36
                                            font.bold: true
                                            color: "white"
                                            width: 300
                                            clip: true
                                            activeFocusOnPress: true
                                            
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
                                                if (text.length > 0) {
                                                    var success = gameController.submitWord(fullWord)
                                                    if (success) {
                                                        text = ""
                                                        hasSubmittedOnce = true
                                                        maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                                                        timeRemaining = maxTime
                                                        timerRunning = true
                                                        forceActiveFocus()
                                                    }
                                                    // If not successful, timer keeps running and input is cleared by signal
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

                    // Top Solves text overlay (only shown after first submission)
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        
                        Rectangle {
                            visible: hasSubmittedOnce && gameController.topSolves.length > 0
                            anchors.centerIn: parent
                            width: topSolvesText.width + 40
                            height: 30
                            color: "#1a1c1f"
                            radius: 8
                            
                            Text {
                                id: topSolvesText
                                anchors.centerIn: parent
                                text: {
                                    if (!hasSubmittedOnce || gameController.topSolves.length === 0) return ""
                                    var solves = "top solves: "
                                    for (var i = 0; i < Math.min(4, gameController.topSolves.length); i++) {
                                        if (i > 0) solves += ", "
                                        solves += gameController.topSolves[i]
                                    }
                                    return solves
                                }
                                font.pixelSize: 16
                                color: "#6495ed"
                            }
                        }
                    }

                    // Main gameplay area - centered avatars with words below
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        RowLayout {
                            anchors.centerIn: parent
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
                                            model: 3
                                            delegate: Rectangle {
                                                width: 40
                                                height: 40
                                                color: index < localPlayerHearts ? "#ff6b9d" : "#5a5d64"
                                                radius: 20

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "♥"
                                                    font.pixelSize: 24
                                                    color: index < localPlayerHearts ? "white" : "#3e4148"
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
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 200
                                    Layout.alignment: Qt.AlignHCenter
                                    color: "#6b6e76"
                                    radius: 100

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Player's\nAvatar"
                                        font.pixelSize: 24
                                        font.bold: true
                                        color: "#f5c542"
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }

                                // Player's last submitted word overlay
                                Item {
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 60
                                    Layout.alignment: Qt.AlignHCenter
                                    
                                    Rectangle {
                                        visible: gameController.playerWords.length > 0
                                        anchors.centerIn: parent
                                        width: Math.min(playerWordText.implicitWidth + 20, 220)
                                        height: 40
                                        color: "#1a1c1f"
                                        radius: 8
                                        
                                        Text {
                                            id: playerWordText
                                            anchors.centerIn: parent
                                            font.pixelSize: 20
                                            font.bold: true
                                            textFormat: Text.RichText
                                            horizontalAlignment: Text.AlignHCenter
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, 200)
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

                            // Star progress display (3x2 grid between player and AI)
                            Item {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 400
                                Layout.alignment: Qt.AlignVCenter

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 15

                                    // Top row - 3 stars
                                    Row {
                                        spacing: 15
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        Repeater {
                                            model: 3
                                            delegate: Text {
                                                text: "★"
                                                font.pixelSize: 48
                                                color: index < gameController.playerPoints ? "#f5c542" : "#5a5d64"
                                                
                                                Behavior on color {
                                                    ColorAnimation { duration: 300 }
                                                }
                                            }
                                        }
                                    }

                                    // Bottom row - 3 stars
                                    Row {
                                        spacing: 15
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        Repeater {
                                            model: 3
                                            delegate: Text {
                                                text: "★"
                                                font.pixelSize: 48
                                                color: (index + 3) < gameController.playerPoints ? "#f5c542" : "#5a5d64"
                                                
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
                                    y: 200
                                    visible: showTopSolveNotification

                                    NumberAnimation on y {
                                        running: showTopSolveNotification
                                        from: 200
                                        to: 140
                                        duration: 2000
                                    }

                                    NumberAnimation on opacity {
                                        running: showTopSolveNotification
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

                                // Spacer for alignment (to match hearts on player side)
                                Item {
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 50
                                }

                                // AI avatar
                                Rectangle {
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 200
                                    Layout.alignment: Qt.AlignHCenter
                                    color: "#6b6e76"
                                    radius: 100

                                    Text {
                                        anchors.centerIn: parent
                                        text: "AI's\nAvatar"
                                        font.pixelSize: 24
                                        font.bold: true
                                        color: "#f5c542"
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }

                                // AI's last submitted word overlay
                                Item {
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 60
                                    Layout.alignment: Qt.AlignHCenter
                                    
                                    Rectangle {
                                        visible: gameController.aiWords.length > 0
                                        anchors.centerIn: parent
                                        width: Math.min(aiWordText.implicitWidth + 20, 220)
                                        height: 40
                                        color: "#1a1c1f"
                                        radius: 8
                                        
                                        Text {
                                            id: aiWordText
                                            anchors.centerIn: parent
                                            text: gameController.aiWords.length > 0 ? gameController.aiWords[gameController.aiWords.length - 1] : ""
                                            font.pixelSize: 20
                                            font.bold: true
                                            color: "white"
                                            horizontalAlignment: Text.AlignHCenter
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, 200)
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
                            text: "End Game"
                            color: "white"
                            font.pixelSize: 18
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
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
                color: "#2b2d31"

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
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 40

                        Button {
                            Layout.preferredWidth: 240
                            Layout.preferredHeight: 70
                            
                            background: Rectangle {
                                color: parent.hovered ? "#f0f0f0" : "white"
                                radius: 12
                                border.color: "white"
                                border.width: 3
                            }

                            contentItem: Text {
                                text: "Yes"
                                font.pixelSize: 32
                                font.bold: true
                                color: "#2b2d31"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                gameController.startNewGame()
                                userInput.text = ""
                                showTopSolveNotification = false
                                hasSubmittedOnce = false
                                localPlayerHearts = gameController.playerHearts
                                maxTime = getMaxTimeForDifficulty(gameController.difficulty)
                                timeRemaining = maxTime
                                timerRunning = true
                                currentScreen = "gameplay"
                                userInput.forceActiveFocus()
                            }
                        }

                        Button {
                            Layout.preferredWidth: 240
                            Layout.preferredHeight: 70
                            
                            background: Rectangle {
                                color: "transparent"
                                radius: 12
                                border.color: "#ff6b6b"
                                border.width: 4
                            }

                            contentItem: Text {
                                text: "No"
                                font.pixelSize: 32
                                font.bold: true
                                color: "#ff6b6b"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: Qt.quit()
                        }
                    }
                }
            }
        }
    }
}
