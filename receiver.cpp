#include "message_queue.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

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
                        std::cerr << "Searched in:" << std::endl;
                        std::cerr << "  " << (exe_dir / "sender.exe").string() << std::endl;
                        std::cerr << "  " << (fs::current_path() / "sender.exe").string() << std::endl;
                        std::cerr << "  " << (exe_dir / "Debug" / "sender.exe").string() << std::endl;
                        std::cerr << "  " << (exe_dir / "Release" / "sender.exe").string() << std::endl;
                        return false;
                    }
                }
            }
        }

        exe_path = sender_path.string();
        std::cout << "Using sender executable: " << exe_path << std::endl;

        for (int i = 0; i < sender_count_; ++i) {
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            std::string cmd = "\"" + exe_path + "\" \"" + filename_ + "\"";

            std::cout << "Starting command: " << cmd << std::endl;

            if (!CreateProcessA(
                exe_path.c_str(),
                const_cast<char*>(cmd.c_str()),
                NULL,
                NULL,
                FALSE,
                CREATE_NEW_CONSOLE,
                NULL,
                NULL, 
                &si,
                &pi)
                ) {
                DWORD error = GetLastError();
                std::cerr << "CreateProcess failed! Error: " << error << std::endl;

                if (error == ERROR_FILE_NOT_FOUND) {
                    std::cerr << "File not found: " << exe_path << std::endl;
                }
                else if (error == ERROR_PATH_NOT_FOUND) {
                    std::cerr << "Path not found for: " << exe_path << std::endl;
                }
                return false;
            }

            sender_processes_.push_back(pi);
            std::cout << "Started Sender process PID: " << pi.dwProcessId << std::endl;

            CloseHandle(pi.hThread);
        }
        return true;
    }

    bool mainLoop() {
        std::cout << "\nCommands:\n  r - read message\n  q - quit\n" << std::endl;

        char command;
        while (true) {
            std::cout << "> ";
            std::cin >> command;

            if (command == 'r') {
                Message msg = queue_.read();
                if (msg.is_empty) {
                    std::cout << "Queue is empty. Waiting..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                else {
                    std::cout << "Received: " << msg.toString() << std::endl;
                }
            }
            else if (command == 'q') {
                break;
            }
        }

        cleanup();
        return true;
    }

    void cleanup() {
        std::cout << "Terminating sender processes..." << std::endl;
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