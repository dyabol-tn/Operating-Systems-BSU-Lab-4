#include <gtest/gtest.h>
#include <filesystem>
#include "message_queue.h"

namespace fs = std::filesystem;

class MessageQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_filename = "test_queue.bin";
        if (fs::exists(test_filename)) {
            fs::remove(test_filename);
        }
    }
    void TearDown() override {
        if (fs::exists(test_filename)) {
            fs::remove(test_filename);
        }
    }
    std::string test_filename;
};

TEST_F(MessageQueueTest, CreateQueue) {
    MessageQueue queue;
    EXPECT_TRUE(queue.create(test_filename, 5));
    auto file_size = fs::file_size(test_filename);
    EXPECT_EQ(file_size, sizeof(QueueHeader) + 5 * sizeof(Message));
}

TEST_F(MessageQueueTest, WriteAndReadSingleMessage) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 3));
    EXPECT_TRUE(queue.write("Hello"));
    Message msg = queue.read();
    EXPECT_FALSE(msg.is_empty);
    EXPECT_EQ(msg.toString(), "Hello");
}

TEST_F(MessageQueueTest, ReadEmptyQueue) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 2));
    Message msg = queue.read();
    EXPECT_TRUE(msg.is_empty);
    EXPECT_EQ(msg.toString(), "");
}

TEST_F(MessageQueueTest, WriteToFullQueue) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 2));
    EXPECT_TRUE(queue.write("First"));
    EXPECT_TRUE(queue.write("Second"));
    EXPECT_FALSE(queue.write("Third"));
}

TEST_F(MessageQueueTest, FIFOOrder) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 3));
    queue.write("Msg1");
    queue.write("Msg2");
    queue.write("Msg3");
    EXPECT_EQ(queue.read().toString(), "Msg1");
    EXPECT_EQ(queue.read().toString(), "Msg2");
    EXPECT_EQ(queue.read().toString(), "Msg3");
}

TEST_F(MessageQueueTest, CircularBuffer) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 2));
    queue.write("A");
    queue.write("B");
    queue.read();
    queue.write("C");
    EXPECT_EQ(queue.read().toString(), "B");
    EXPECT_EQ(queue.read().toString(), "C");
}

TEST_F(MessageQueueTest, OpenExistingQueue) {
    {
        MessageQueue queue;
        queue.create(test_filename, 3);
        queue.write("Test1");
        queue.write("Test2");
    }
    {
        MessageQueue queue;
        ASSERT_TRUE(queue.open(test_filename));
        EXPECT_EQ(queue.read().toString(), "Test1");
        EXPECT_EQ(queue.read().toString(), "Test2");
        EXPECT_TRUE(queue.isEmpty());
    }
}

TEST_F(MessageQueueTest, IsEmptyAndIsFull) {
    MessageQueue queue;
    ASSERT_TRUE(queue.create(test_filename, 2));
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
    queue.write("First");
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
    queue.write("Second");
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_TRUE(queue.isFull());
    queue.read();
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
}
