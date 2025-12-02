#include "message_queue.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

class Receiver {
private:
    std::string filename_;
    int capacity_;
    int sender_count_;
    MessageQueue queue_;
    std::vector<PROCESS_INFORMATION> sender_processes_;
public:
    bool run() {
        if (!setup()) return false;
        if (!startSenders()) return false;
        if (!waitForSendersReady()) return false;
        return mainLoop();
    }
private:
    bool setup() {
        std::cout << "Enter binary filename: ";
        std::cin >> filename_;
        std::cout << "Enter queue capacity: ";
        std::cin >> capacity_;
        if (capacity_ <= 0) {
            std::cerr << "Error: capacity must be > 0" << std::endl;
            return false;
        }
        if (!queue_.create(filename_, capacity_)) {
            std::cerr << "Error creating file" << std::endl;
            return false;
        }
        std::cout << "Enter number of Sender processes: ";
        std::cin >> sender_count_;
        return true;
    }
    bool startSenders() {
        std::string exe_path;
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        fs::path full_path(buffer);
        fs::path exe_dir = full_path.parent_path();
        fs::path sender_path = exe_dir / "sender.exe";
        if (!fs::exists(sender_path)) {
            sender_path = "sender.exe";
            if (!fs::exists(sender_path)) {
                sender_path = exe_dir / "Debug" / "sender.exe";
                if (!fs::exists(sender_path)) {
                    sender_path = exe_dir / "Release" / "sender.exe";
                    if (!fs::exists(sender_path)) {
                        std::cerr << "Error: sender.exe not found!" << std::endl;
                        return false;
                    }
                }
            }
        }
        exe_path = sender_path.string();
        for (int i = 0; i < sender_count_; ++i) {
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            std::string cmd = "\"" + exe_path + "\" \"" + filename_ + "\"";
            char* cmdLine = _strdup(cmd.c_str());
            if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
                std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
                free(cmdLine);
                return false;
            }
            free(cmdLine);
            sender_processes_.push_back(pi);
            CloseHandle(pi.hThread);
        }
        return true;
    }
    bool waitForSendersReady() {
        std::cout << "Waiting for Sender processes to be ready..." << std::endl;
        if (!queue_.waitForReady(10000)) {
            std::cerr << "Timeout waiting for Sender processes" << std::endl;
            return false;
        }
        std::cout << "All Sender processes are ready" << std::endl;
        return true;
    }
    bool mainLoop() {
        std::cout << "\nCommands:\n  r - read message\n  s - show status\n  q - quit\n" << std::endl;
        char command;
        while (true) {
            std::cout << "> ";
            std::cin >> command;
            if (command == 'r') {
                Message msg = queue_.read(1000);
                if (msg.is_empty) {
                    std::cout << "Queue is empty or timeout." << std::endl;
                    if (queue_.getCount() < 0) {
                        std::cout << "Warning: Queue count is negative. Try restarting." << std::endl;
                    }
                }
                else {
                    std::cout << "Received: " << msg.toString() << std::endl;
                }
            }
            else if (command == 's') {
                std::cout << "Queue status:" << std::endl;
                std::cout << "  Capacity: " << queue_.getCapacity() << std::endl;
                std::cout << "  Count: " << queue_.getCount() << std::endl;
                std::cout << "  Is empty: " << (queue_.isEmpty() ? "Yes" : "No") << std::endl;
                std::cout << "  Is full: " << (queue_.isFull() ? "Yes" : "No") << std::endl;
            }
            else if (command == 'q') {
                break;
            }
            else {
                std::cout << "Unknown command. Use 'r' to read, 's' for status, or 'q' to quit." << std::endl;
            }
        }
        cleanup();
        return true;
    }
    void cleanup() {
        for (auto& pi : sender_processes_) {
            if (pi.hProcess) {
                TerminateProcess(pi.hProcess, 0);
                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
            }
        }
        sender_processes_.clear();
    }
};

int main() {
    Receiver receiver;
    return receiver.run() ? 0 : 1;
}
