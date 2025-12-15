#include "message_queue.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <process.h>

#ifdef _WIN32
#include <windows.h>
#endif

class Receiver {
private:
    std::string filename_;
    int capacity_;
    int sender_count_;
    MessageQueue queue_;
    std::vector<HANDLE> sender_processes_;

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
        for (int i = 0; i < sender_count_; ++i) {
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            std::string cmd = "sender.exe " + filename_;

            if (!CreateProcessA(
                NULL,
                const_cast<char*>(cmd.c_str()),
                NULL,
                NULL,
                FALSE,
                0,
                NULL,
                NULL,
                &si,
                &pi)
                ) {
                std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
                return false;
            }

            sender_processes_.push_back(pi.hProcess);
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
        for (HANDLE process : sender_processes_) {
            TerminateProcess(process, 0);
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
        }
    }
};

int main() {
    Receiver receiver;
    return receiver.run() ? 0 : 1;
}