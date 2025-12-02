#include "message_queue.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

class Sender {
private:
    std::string filename_;
    MessageQueue queue_;
public:
    Sender(const std::string& filename) : filename_(filename) {
        std::cout << "Sender created with filename: " << filename_ << std::endl;
    }
    bool run() {
        if (!setup()) {
            std::cerr << "Setup failed!" << std::endl;
            return false;
        }
        return mainLoop();
    }
private:
    bool setup() {
        std::cout << "Checking if file exists: " << filename_ << std::endl;
        if (!fs::exists(filename_)) {
            std::cerr << "ERROR: File does not exist: " << filename_ << std::endl;
            char current_dir[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, current_dir);
            std::cerr << "Current directory: " << current_dir << std::endl;
            std::cerr << "Files in current directory:" << std::endl;
            for (const auto& entry : fs::directory_iterator(".")) {
                std::cerr << "  " << entry.path().filename() << std::endl;
            }
            return false;
        }
        std::cout << "File exists. Opening queue..." << std::endl;
        if (!queue_.open(filename_)) {
            std::cerr << "Failed to open queue!" << std::endl;
            return false;
        }
        std::cout << "Sending ready signal..." << std::endl;
        if (!queue_.signalReady()) {
            std::cerr << "Failed to send ready signal!" << std::endl;
            return false;
        }
        std::cout << "Connected to queue: " << filename_ << std::endl;
        return true;
    }
    bool mainLoop() {
        std::cout << "\n=== Sender Commands ===\n  s - send message\n  q - quit\n" << std::endl;
        char command;
        std::string message;
        while (true) {
            std::cout << "> ";
            std::cin >> command;
            if (command == 's') {
                std::cout << "Enter message (max " << (MAX_MESSAGE_LENGTH - 1) << " chars): ";
                std::cin.ignore();
                std::getline(std::cin, message);
                if (message.length() > MAX_MESSAGE_LENGTH - 1) {
                    std::cout << "Error: Message too long (" << message.length() << " chars, max " << (MAX_MESSAGE_LENGTH - 1) << ")" << std::endl;
                    continue;
                }
                std::cout << "Attempting to send message: \"" << message << "\"" << std::endl;
                if (!queue_.write(message, 5000)) {
                    std::cout << "Queue is full. Waiting 1 second..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                else {
                    std::cout << "Successfully sent: \"" << message << "\"" << std::endl;
                }
            }
            else if (command == 'q') {
                std::cout << "Quitting sender..." << std::endl;
                break;
            }
            else {
                std::cout << "Unknown command. Use 's' to send or 'q' to quit." << std::endl;
            }
        }
        return true;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== Sender Process Started ===" << std::endl;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        char current_dir[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, current_dir);
        std::cerr << "Current directory: " << current_dir << std::endl;
        return 1;
    }
    std::cout << "Received filename argument: \"" << argv[1] << "\"" << std::endl;
    Sender sender(argv[1]);
    int result = sender.run() ? 0 : 1;
    std::cout << "Sender process exiting with code: " << result << std::endl;
    std::cout << "Press Enter to close this window..." << std::endl;
    std::cin.ignore();
    std::cin.get();
    return result;
}
