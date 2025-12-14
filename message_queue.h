#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <string>
#include <fstream>
#include <iostream>
#include <cstring>

constexpr int MAX_MESSAGE_LENGTH = 20;

#pragma pack(push, 1)
struct Message {
    bool is_empty;
    char text[MAX_MESSAGE_LENGTH];

    Message() : is_empty(true) { text[0] = '\0'; }
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
    bool is_open;

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
    MessageQueue() : is_open(false), header{ 0,0,0,0 } {}

    bool create(const std::string& fname, int capacity) {
        filename = fname;
        header = { capacity, 0, 0, 0 };

        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file) return false;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        Message empty_msg;
        for (int i = 0; i < capacity; ++i) {
            file.write(reinterpret_cast<const char*>(&empty_msg), sizeof(Message));
        }

        is_open = file.good();
        return is_open;
    }

    bool open(const std::string& fname) {
        filename = fname;
        is_open = readHeader();
        return is_open;
    }

    bool write(const std::string& message) {
        if (!is_open || isFull()) return false;

        std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) return false;

        Message msg(message);

        file.seekp(sizeof(QueueHeader) + header.tail * sizeof(Message));
        file.write(reinterpret_cast<const char*>(&msg), sizeof(Message));

        header.tail = (header.tail + 1) % header.capacity;
        header.count++;

        writeHeader();
        return true;
    }

    Message read() {
        Message msg;
        if (!is_open || isEmpty()) return msg;

        std::fstream file(filename, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) return msg;

        file.seekg(sizeof(QueueHeader) + header.head * sizeof(Message));
        file.read(reinterpret_cast<char*>(&msg), sizeof(Message));

        if (!msg.is_empty) {
            msg.is_empty = true;
            file.seekp(sizeof(QueueHeader) + header.head * sizeof(Message));
            file.write(reinterpret_cast<const char*>(&msg), sizeof(Message));

            header.head = (header.head + 1) % header.capacity;
            header.count--;
            writeHeader();
        }

        return msg;
    }

    bool isEmpty() const { return header.count == 0; }
    bool isFull() const { return header.count >= header.capacity; }
    void close() { is_open = false; }
};

#endif