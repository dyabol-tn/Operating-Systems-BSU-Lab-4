#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <windows.h>
#include <filesystem>

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
    std::string mappingName;
    QueueHeader header;
    HANDLE hFileMap;
    QueueHeader* pMappedHeader;
    Message* pMappedMessages;
    HANDLE hSemEmpty;
    HANDLE hSemFull;
    HANDLE hMutex;
    HANDLE hReadyEvent;
    static std::string canonicalizePath(const std::string& p) {
        try {
            return std::filesystem::absolute(p).string();
        }
        catch (...) {
            return p;
        }
    }
    std::string getSyncObjectName(const std::string& base, const std::string& type) {
        std::string name = base + "_" + type;
        for (char& c : name) {
            if (c == ':' || c == '\\' || c == '/' || c == ' ' || c == '.') c = '_';
        }
        return std::string("Global\\") + name;
    }
    bool createSyncObjects() {
        std::string base_name = filename;
        for (char& c : base_name) {
            if (c == ':' || c == '\\' || c == '/') c = '_';
        }
        std::string nameEmpty = getSyncObjectName(base_name, "empty");
        std::cout << "CreateSyncObjects: creating '" << nameEmpty << "'" << std::endl;
        hSemEmpty = CreateSemaphoreA(NULL, header.capacity, header.capacity, nameEmpty.c_str());
        if (hSemEmpty == NULL) {
            std::cerr << "CreateSemaphore empty failed: " << GetLastError() << std::endl;
            return false;
        }
        std::string nameFull = getSyncObjectName(base_name, "full");
        std::cout << "CreateSyncObjects: creating '" << nameFull << "'" << std::endl;
        hSemFull = CreateSemaphoreA(NULL, 0, header.capacity, nameFull.c_str());
        if (hSemFull == NULL) {
            std::cerr << "CreateSemaphore full failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            hSemEmpty = NULL;
            return false;
        }
        std::string nameMutex = getSyncObjectName(base_name, "mutex");
        std::cout << "CreateSyncObjects: creating '" << nameMutex << "'" << std::endl;
        hMutex = CreateMutexA(NULL, FALSE, nameMutex.c_str());
        if (hMutex == NULL) {
            std::cerr << "CreateMutex failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            hSemEmpty = NULL;
            hSemFull = NULL;
            return false;
        }
        std::string nameReady = getSyncObjectName(base_name, "ready");
        std::cout << "CreateSyncObjects: creating '" << nameReady << "'" << std::endl;
        hReadyEvent = CreateEventA(NULL, TRUE, FALSE, nameReady.c_str());
        if (hReadyEvent == NULL) {
            std::cerr << "CreateEvent failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            CloseHandle(hMutex);
            hSemEmpty = NULL;
            hSemFull = NULL;
            hMutex = NULL;
            return false;
        }
        return true;
    }
    bool openSyncObjects() {
        std::string base_name = filename;
        for (char& c : base_name) {
            if (c == ':' || c == '\\' || c == '/') c = '_';
        }
        std::string nameEmpty = getSyncObjectName(base_name, "empty");
        std::cout << "OpenSyncObjects: opening '" << nameEmpty << "'" << std::endl;
        hSemEmpty = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, nameEmpty.c_str());
        if (hSemEmpty == NULL) {
            std::cerr << "OpenSemaphore empty failed: " << GetLastError() << std::endl;
            return false;
        }
        std::string nameFull = getSyncObjectName(base_name, "full");
        std::cout << "OpenSyncObjects: opening '" << nameFull << "'" << std::endl;
        hSemFull = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, nameFull.c_str());
        if (hSemFull == NULL) {
            std::cerr << "OpenSemaphore full failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            hSemEmpty = NULL;
            return false;
        }
        std::string nameMutex = getSyncObjectName(base_name, "mutex");
        std::cout << "OpenSyncObjects: opening '" << nameMutex << "'" << std::endl;
        hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, nameMutex.c_str());
        if (hMutex == NULL) {
            std::cerr << "OpenMutex failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            hSemEmpty = NULL;
            hSemFull = NULL;
            return false;
        }
        std::string nameReady = getSyncObjectName(base_name, "ready");
        std::cout << "OpenSyncObjects: opening '" << nameReady << "'" << std::endl;
        hReadyEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, nameReady.c_str());
        if (hReadyEvent == NULL) {
            std::cerr << "OpenEvent failed: " << GetLastError() << std::endl;
            CloseHandle(hSemEmpty);
            CloseHandle(hSemFull);
            CloseHandle(hMutex);
            hSemEmpty = NULL;
            hSemFull = NULL;
            hMutex = NULL;
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
        if (hReadyEvent != NULL) {
            CloseHandle(hReadyEvent);
            hReadyEvent = NULL;
        }
    }

public:
    MessageQueue() : hFileMap(NULL), pMappedHeader(NULL), pMappedMessages(NULL), hSemEmpty(NULL), hSemFull(NULL), hMutex(NULL), hReadyEvent(NULL) {}
    ~MessageQueue() {
        if (pMappedHeader != NULL) {
            UnmapViewOfFile(pMappedHeader);
            pMappedHeader = NULL;
            pMappedMessages = NULL;
        }
        if (hFileMap != NULL) {
            CloseHandle(hFileMap);
            hFileMap = NULL;
        }
        closeSyncObjects();
    }
    bool create(const std::string& fname, int capacity) {
        filename = canonicalizePath(fname);
        header = { capacity, 0, 0, 0 };
        std::cout << "Creating queue file: " << filename << " with capacity: " << capacity << std::endl;
        HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateFile failed: " << GetLastError() << std::endl;
            return false;
        }
        LARGE_INTEGER fileSize;
        fileSize.QuadPart = sizeof(QueueHeader) + capacity * sizeof(Message);
        SetFilePointerEx(hFile, fileSize, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
        std::string base_name = filename;
        for (char& c : base_name) if (c == ':' || c == '\\' || c == '/') c = '_';
        mappingName = std::string("Global\\mq_map_") + base_name;
        hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, mappingName.c_str());
        CloseHandle(hFile);
        if (hFileMap == NULL) {
            std::cerr << "CreateFileMapping failed: " << GetLastError() << std::endl;
            return false;
        }
        pMappedHeader = (QueueHeader*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (pMappedHeader == NULL) {
            std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
            CloseHandle(hFileMap);
            hFileMap = NULL;
            return false;
        }
        *pMappedHeader = header;
        pMappedMessages = (Message*)(pMappedHeader + 1);
        for (int i = 0; i < capacity; ++i) {
            pMappedMessages[i] = Message();
        }
        if (!createSyncObjects()) {
            std::cerr << "Failed to create synchronization objects" << std::endl;
            return false;
        }
        std::cout << "Queue created successfully" << std::endl;
        return true;
    }
    bool open(const std::string& fname) {
        filename = canonicalizePath(fname);
        std::cout << "Opening queue file: " << filename << std::endl;
        HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateFile failed: " << GetLastError() << std::endl;
            return false;
        }
        std::string base_name = filename;
        for (char& c : base_name) if (c == ':' || c == '\\' || c == '/') c = '_';
        mappingName = std::string("Global\\mq_map_") + base_name;
        hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, mappingName.c_str());
        CloseHandle(hFile);
        if (hFileMap == NULL) {
            std::cerr << "CreateFileMapping failed: " << GetLastError() << std::endl;
            return false;
        }
        pMappedHeader = (QueueHeader*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (pMappedHeader == NULL) {
            std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
            CloseHandle(hFileMap);
            hFileMap = NULL;
            return false;
        }
        pMappedMessages = (Message*)(pMappedHeader + 1);
        header = *pMappedHeader;
        std::cout << "Queue info - capacity: " << pMappedHeader->capacity << ", count: " << pMappedHeader->count << ", head: " << pMappedHeader->head << ", tail: " << pMappedHeader->tail << std::endl;
        if (!openSyncObjects()) {
            std::cerr << "Failed to open synchronization objects" << std::endl;
            return false;
        }
        return true;
    }
    bool signalReady() {
        std::cout << "Signaling ready event" << std::endl;
        return SetEvent(hReadyEvent) != FALSE;
    }
    bool waitForReady(DWORD timeout = INFINITE) {
        std::cout << "Waiting for ready event..." << std::endl;
        DWORD result = WaitForSingleObject(hReadyEvent, timeout);
        if (result == WAIT_OBJECT_0) {
            std::cout << "Ready event received" << std::endl;
            return true;
        }
        else {
            std::cerr << "Wait for ready event failed or timeout: " << result << std::endl;
            return false;
        }
    }
    bool write(const std::string& message, DWORD timeout = INFINITE) {
        if (message.length() > MAX_MESSAGE_LENGTH - 1) {
            std::cerr << "Message too long: " << message.length() << " (max " << (MAX_MESSAGE_LENGTH - 1) << ")" << std::endl;
            return false;
        }
        DWORD waitResult = WaitForSingleObject(hSemEmpty, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) {
                std::cout << "Write timeout - queue full" << std::endl;
            }
            else {
                std::cerr << "Failed to wait for empty semaphore: " << waitResult << std::endl;
            }
            return false;
        }
        waitResult = WaitForSingleObject(hMutex, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            std::cerr << "Failed to wait for mutex: " << waitResult << std::endl;
            ReleaseSemaphore(hSemEmpty, 1, NULL);
            return false;
        }
        if (pMappedHeader->count >= pMappedHeader->capacity) {
            std::cerr << "Queue is full but semaphore was acquired" << std::endl;
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemEmpty, 1, NULL);
            return false;
        }
        int tail = pMappedHeader->tail;
        if (!pMappedMessages[tail].is_empty) {
            std::cerr << "ERROR: Cell at tail " << tail << " is not empty!" << std::endl;
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemEmpty, 1, NULL);
            return false;
        }
        pMappedMessages[tail] = Message(message);
        pMappedHeader->tail = (tail + 1) % pMappedHeader->capacity;
        pMappedHeader->count++;
        FlushViewOfFile(pMappedHeader, 0);
        header = *pMappedHeader;
        ReleaseMutex(hMutex);
        if (!ReleaseSemaphore(hSemFull, 1, NULL)) {
            std::cerr << "Failed to release full semaphore: " << GetLastError() << std::endl;
            return false;
        }
        std::cout << "Message written successfully. New count: " << pMappedHeader->count << ", tail: " << pMappedHeader->tail << std::endl;
        return true;
    }
    Message read(DWORD timeout = INFINITE) {
        Message emptyMsg;
        DWORD waitResult = WaitForSingleObject(hSemFull, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) {
                std::cout << "Read timeout - no messages available" << std::endl;
            }
            else {
                std::cerr << "Failed to wait for full semaphore: " << waitResult << std::endl;
            }
            return emptyMsg;
        }
        waitResult = WaitForSingleObject(hMutex, timeout);
        if (waitResult != WAIT_OBJECT_0) {
            std::cerr << "Failed to wait for mutex: " << waitResult << std::endl;
            ReleaseSemaphore(hSemFull, 1, NULL);
            return emptyMsg;
        }
        if (pMappedHeader->count <= 0) {
            std::cout << "Queue is empty, but full semaphore was signaled" << std::endl;
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemFull, 1, NULL);
            return emptyMsg;
        }
        int head = pMappedHeader->head;
        Message msg = pMappedMessages[head];
        if (msg.is_empty) {
            std::cout << "Read empty message from position " << head << std::endl;
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hSemFull, 1, NULL);
            return emptyMsg;
        }
        pMappedMessages[head] = Message();
        pMappedHeader->head = (head + 1) % pMappedHeader->capacity;
        pMappedHeader->count--;
        FlushViewOfFile(pMappedHeader, 0);
        header = *pMappedHeader;
        ReleaseMutex(hMutex);
        if (!ReleaseSemaphore(hSemEmpty, 1, NULL)) {
            std::cerr << "Failed to release empty semaphore: " << GetLastError() << std::endl;
        }
        std::cout << "Message read successfully: " << msg.toString() << ", count: " << pMappedHeader->count << ", head: " << pMappedHeader->head << std::endl;
        return msg;
    }
    bool isEmpty() const {
        return pMappedHeader->count == 0;
    }
    bool isFull() const {
        return pMappedHeader->count >= pMappedHeader->capacity;
    }
    int getCapacity() const {
        return pMappedHeader->capacity;
    }
    int getCount() const {
        return pMappedHeader->count;
    }
};

#endif
