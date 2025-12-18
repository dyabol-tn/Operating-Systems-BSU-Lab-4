#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <windows.h>
#include <tchar.h>

constexpr int MAX_MESSAGE_LENGTH = 20;

#pragma pack(push, 1)
struct Message {
    bool is_empty;
    char text[MAX_MESSAGE_LENGTH];

    Message() : is_empty(true) {
        memset(text, 0, sizeof(text));
    }

    Message(const std::string& str) : is_empty(false) {
        strncpy(text, str.c_str(), MAX_MESSAGE_LENGTH - 1);
        text[MAX_MESSAGE_LENGTH - 1] = '\0';
    }

    std::string toString() const {
        return is_empty ? "" : std::string(text);
    }
};

struct QueueHeader {
    int capacity;
    int count;
    int head;
    int tail;
};
#pragma pack(pop)

class MessageQueue {
private:
    std::string filename;
    QueueHeader header;

    HANDLE hSemEmpty;
    HANDLE hSemFull;
    HANDLE hMutex;

    std::string getSemaphoreName(const std::string& base, const std::string& type) {
        return "Global\\" + base + "_" + type;
    }

    bool createSyncObjects() {
        std::string base_name = filename;
        for (char& c : base_name) {
            if (c == ':' || c == '\\' || c == '/') c = '_';
        }

        hSemEmpty = CreateSemaphoreA(
            NULL,
            header.capacity,
            header.capacity,
            getSemaphoreName(base_name, "empty").c_str()
        );

        if (hSemEmpty == NULL) {
            std::cerr << "Failed to create empty semaphore. Error: " << GetLastError() << std::endl;
            return false;
        }

        hSemFull = CreateSemaphoreA(
            NULL,
            0,
            header.capacity,
            getSemaphoreName(base_name, "full").c_str()
        );

        if (hSemFull == NULL) {
            std::cerr << "Failed to create full semaphore. Error: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            return false;
        }

        hMutex = CreateMutexA(
            NULL,
            FALSE,
            getSemaphoreName(base_name, "mutex").c_str()
        );

        if (hMutex == NULL) {
            std::cerr << "Failed to create mutex. Error: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            return false;
        }

        return true;
    }

    bool openSyncObjects() {
        std::string base_name = filename;
        for (char& c : base_name) {
            if (c == ':' || c == '\\' || c == '/') c = '_';
        }

        hSemEmpty = OpenSemaphoreA(
            SEMAPHORE_ALL_ACCESS,
            FALSE,
            getSemaphoreName(base_name, "empty").c_str()
        );

        if (hSemEmpty == NULL) {
            std::cerr << "Failed to open empty semaphore. Error: " << GetLastError() << std::endl;
            return false;
        }

        hSemFull = OpenSemaphoreA(
            SEMAPHORE_ALL_ACCESS,
            FALSE,
            getSemaphoreName(base_name, "full").c_str()
        );

        if (hSemFull == NULL) {
            std::cerr << "Failed to open full semaphore. Error: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            return false;
        }

        hMutex = OpenMutexA(
            MUTEX_ALL_ACCESS,
            FALSE,
            getSemaphoreName(base_name, "mutex").c_str()
        );

        if (hMutex == NULL) {
            std::cerr << "Failed to open mutex. Error: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            return false;
        }

        return true;
    }

    void closeSyncObjects() {
        if (hSemEmpty != NULL) {
            CloseHandle(hSemEmpty);
            hSemEmpty = NULL;
        }
        if (hSemFull != NULL) {
            CloseHandle(hSemFull);
            hSemFull = NULL;
        }
        if (hMutex != NULL) {
            CloseHandle(hMutex);
            hMutex = NULL;
        }
    }

    bool readHeader() {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        return file.good();
    }

    bool writeHeader() {
        std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) return false;
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        return file.good();
    }

public:
    MessageQueue() : hSemEmpty(NULL), hSemFull(NULL), hMutex(NULL), header{ 0, 0, 0, 0 } {}

    ~MessageQueue() {
        closeSyncObjects();
    }

    bool create(const std::string& fname, int capacity) {
        filename = fname;
        header = { capacity, 0, 0, 0 };

        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::cerr << "Failed to create file: " << filename << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        Message empty_msg;
        for (int i = 0; i < capacity; ++i) {
            file.write(reinterpret_cast<const char*>(&empty_msg), sizeof(Message));
        }

        if (!file.good()) {
            std::cerr << "Failed to write initial data to file" << std::endl;
            return false;
        }
        file.close();

        if (!createSyncObjects()) {
            return false;
        }

        return true;
    }

    bool open(const std::string& fname) {
        filename = fname;

        if (!readHeader()) {
            std::cerr << "Failed to read queue header from: " << filename << std::endl;
            return false;
        }

        if (!openSyncObjects()) {
            std::cerr << "Failed to open synchronization objects for: " << filename << std::endl;
            return false;
        }

        return true;
    }

    bool write(const std::string& message, DWORD timeout = INFINITE) {
        DWORD waitResult = WaitForSingleObject(hSemEmpty, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) {
                std::cerr << "Timeout waiting for empty slot" << std::endl;
            }
            else {
                std::cerr << "Failed to wait for empty semaphore. Error: " << GetLastError() << std::endl;
            }
            return false;
        }

        waitResult = WaitForSingleObject(hMutex, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            ReleaseSemaphore(hSemEmpty, 1, NULL);
            return false;
        }

        bool success = false;
        do {
            std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
            if (!file) {
                std::cerr << "Failed to open file for writing" << std::endl;
                break;
            }

            Message msg(message);

            file.seekp(sizeof(QueueHeader) + header.tail * sizeof(Message));
            file.write(reinterpret_cast<const char*>(&msg), sizeof(Message));

            header.tail = (header.tail + 1) % header.capacity;
            header.count++;

            file.seekp(0);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));

            if (!file.good()) {
                std::cerr << "Failed to write to file" << std::endl;
                break;
            }

            success = true;
        } while (false);

        ReleaseMutex(hMutex);

        if (success) {
            if (!ReleaseSemaphore(hSemFull, 1, NULL)) {
                std::cerr << "Failed to release full semaphore. Error: " << GetLastError() << std::endl;
            }
        }
        else {
            ReleaseSemaphore(hSemEmpty, 1, NULL);
        }

        return success;
    }

    Message read(DWORD timeout = INFINITE) {
        Message msg;

        DWORD waitResult = WaitForSingleObject(hSemFull, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) {
                std::cerr << "Timeout waiting for message" << std::endl;
            }
            else {
                std::cerr << "Failed to wait for full semaphore. Error: " << GetLastError() << std::endl;
            }
            return msg;
        }

        waitResult = WaitForSingleObject(hMutex, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            ReleaseSemaphore(hSemFull, 1, NULL);
            return msg;
        }

        do {
            std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
            if (!file) {
                std::cerr << "Failed to open file for reading" << std::endl;
                break;
            }

            file.seekg(sizeof(QueueHeader) + header.head * sizeof(Message));
            file.read(reinterpret_cast<char*>(&msg), sizeof(Message));

            if (!msg.is_empty) {
                msg.is_empty = true;
                file.seekp(sizeof(QueueHeader) + header.head * sizeof(Message));
                file.write(reinterpret_cast<const char*>(&msg), sizeof(Message));

                header.head = (header.head + 1) % header.capacity;
                header.count--;

                file.seekp(0);
                file.write(reinterpret_cast<const char*>(&header), sizeof(header));

                if (!file.good()) {
                    std::cerr << "Failed to update file after reading" << std::endl;
                    msg.is_empty = true;
                    break;
                }
            }
        } while (false);

        ReleaseMutex(hMutex);

        if (!msg.is_empty) {
            if (!ReleaseSemaphore(hSemEmpty, 1, NULL)) {
                std::cerr << "Failed to release empty semaphore. Error: " << GetLastError() << std::endl;
            }
        }
        else {
            ReleaseSemaphore(hSemFull, 1, NULL);
        }

        return msg;
    }

    bool isEmpty() const {
        return header.count == 0;
    }

    bool isFull() const {
        return header.count >= header.capacity;
    }

    int getCapacity() const {
        return header.capacity;
    }

    int getCount() const {
        return header.count;
    }
};

inline bool startSenderProcess(const std::string& filename, PROCESS_INFORMATION& pi) {
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string command = "sender.exe \"" + filename + "\"";

    if (!CreateProcessA(
        NULL,
        const_cast<char*>(command.c_str()),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        std::cerr << "CreateProcess failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

#endif