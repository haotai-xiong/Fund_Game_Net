#define  _WINSOCK_DEPRECATED_NO_WARNINGS
#define MY_PORT 56000
#define MY_IP "192.168.1.15"

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <WinSock2.h>
#include <thread>
#include <mutex>
#include <iostream>

struct Player {
    sf::Vector2f position;
    bool isIt;

    void move(const sf::Vector2f& direction) {
        position += direction;
    }
};

struct GameState {
    Player players[3];
    bool gameRunning;
};

void sendPlayerPosition(SOCKET clientSocket, Player* player) {
    while (true) {
        char positionBuffer[sizeof(sf::Vector2f)];
        sf::Vector2f playerPosition = player->position;
        memcpy(positionBuffer, &playerPosition, sizeof(sf::Vector2f));
        send(clientSocket, positionBuffer, sizeof(sf::Vector2f), 0);

        // dont set too low so server isn't overwheled
        Sleep(25);
    }
}

void receiveGameState(SOCKET clientSocket, GameState& gameState, int clientIndex, std::mutex& gameStateMutex) {
    while (true) {
        char gameStateBuffer[sizeof(GameState)];
        int bytesReceived = recv(clientSocket, gameStateBuffer, sizeof(GameState), 0);

        if (bytesReceived <= 0) {
            // todo (server probably closed)
            break;
        }

        std::lock_guard<std::mutex> lock(gameStateMutex);
        memcpy(&gameState, gameStateBuffer, sizeof(GameState));
    }
}

// draw all players
void drawPlayers(sf::RenderWindow& window, const GameState& gameState, int clientIndex) {
    for (int i = 0; i < 3; ++i) {
        sf::RectangleShape playerShape(sf::Vector2f(50.0f, 50.0f)); // not very efficient but we ball
        playerShape.setPosition(gameState.players[i].position);

        if (i == clientIndex) {
            playerShape.setFillColor(sf::Color::Green); // Current player
        }
        else if (gameState.players[i].isIt) {
            playerShape.setFillColor(sf::Color::Red); // Player that is "it"
        }
        else {
            playerShape.setFillColor(sf::Color::White); // Other player
        }

        window.draw(playerShape);
    }
}

int main()
{
    std::cout << "Top of main function" << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize WinSock." << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(MY_PORT); // port
    serverAddr.sin_addr.s_addr = inet_addr(MY_IP); // ip


    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to the server." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    int clientIndex;
    recv(clientSocket, reinterpret_cast<char*>(&clientIndex), sizeof(int), 0);
    std::cout << "Connected as client " << clientIndex << std::endl;

    Player player;
    player.position = sf::Vector2f(0, 0); // Initial position for debug
    player.isIt = false; // Initially the player is not "it" for debug

    GameState gameState;
    std::mutex gameStateMutex;

    // thread for sending player position to the server
    std::thread sendThread(sendPlayerPosition, clientSocket, &player);
    // thread for receiving game state updates from the server
    std::thread receiveThread(receiveGameState, clientSocket, std::ref(gameState), clientIndex, std::ref(gameStateMutex));

    sf::RenderWindow window(sf::VideoMode(800, 600), "Tag Game");

    while (true) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // Handle player movement
        sf::Vector2f direction(0.0f, 0.0f);
        const float moveSpeed = 5.0f;
        if (window.hasFocus())
        {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) direction.y -= moveSpeed;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) direction.y += moveSpeed;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) direction.x -= moveSpeed;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) direction.x += moveSpeed;
        }
        {
            std::lock_guard<std::mutex> lock(gameStateMutex);
            player.move(direction);  // Update players position
        }

        window.clear(sf::Color::Black);
        drawPlayers(window, gameState, clientIndex);
        window.display();

        Sleep(25);
    }

    // Clean up
    if (sendThread.joinable()) {
        sendThread.join();
    }
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    closesocket(clientSocket);
    WSACleanup();
    system("pause");
    return 1; // success
}