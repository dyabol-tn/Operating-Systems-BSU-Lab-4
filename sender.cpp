#include "message_queue.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

class Sender {
private:
    std::string filename_;
    MessageQueue queue_;

public:
    Sender(const std::string& filename) : filename_(filename) {}

    bool run() {
        if (!setup()) return false;
        return mainLoop();
    }

private:
    bool setup() {
        if (!queue_.open(filename_)) {
            std::cerr << "Error opening file: " << filename_ << std::endl;
            return false;
        }

        std::cout << "Connected to queue: " << filename_ << std::endl;
        return true;
    }

    bool mainLoop() {
        std::cout << "\nCommands:\n  s - send message\n  q - quit\n" << std::endl;

        char command;
        std::string message;

        while (true) {
            std::cout << "> ";
            std::cin >> command;

            if (command == 's') {
                std::cout << "Enter message (max " << MAX_MESSAGE_LENGTH << " chars): ";
                std::cin.ignore();
                std::getline(std::cin, message);

                if (message.length() >= MAX_MESSAGE_LENGTH) {
                    message = message.substr(0, MAX_MESSAGE_LENGTH - 1);
                    std::cout << "Message truncated to: " << message << std::endl;
                }

                if (!queue_.write(message)) {
                    std::cout << "Queue is full. Waiting..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (!queue_.write(message)) {
                        std::cout << "Still full, skipping." << std::endl;
                    }
                }
                else {
                    std::cout << "Sent: " << message << std::endl;
                }
            }
            else if (command == 'q') {
                break;
            }
        }

        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    Sender sender(argv[1]);
    return sender.run() ? 0 : 1;
}