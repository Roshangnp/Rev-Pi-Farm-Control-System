#pragma once
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// Sends push notifications through ntfy (https://ntfy.sh, or a self-hosted server).
// Anyone who knows the topic name can read and post to it, so use a long random one.
//
// send() only queues the message and returns at once; a background thread posts
// it with the curl program (HTTPS, 10 s timeout, 3 tries), so a slow or missing
// internet connection never blocks the caller. Failures are logged to stderr.
class ntfy_notifier{
    public:
    // Empty topic = disabled (send() returns false)
    explicit ntfy_notifier(std::string topic, std::string server = "https://ntfy.sh");
    // Waits for the message being posted, drops the rest of the queue
    ~ntfy_notifier();

    ntfy_notifier(const ntfy_notifier&) = delete;
    ntfy_notifier& operator=(const ntfy_notifier&) = delete;

    // priority: 1 (min) .. 3 (default) .. 5 (max, bypasses Do Not Disturb on the phone)
    // tags: comma separated ntfy tags; emoji names like "warning" show as icons
    // False if disabled or the queue is full
    bool send(const std::string& title, const std::string& message, int priority = 3,
              const std::string& tags = "");

    // Waits until every queued message has been posted (or given up on).
    // False on timeout.
    bool flush(std::chrono::milliseconds timeout);

    bool enabled() const;
    size_t sentCount() const;
    size_t failedCount() const;

    private:
    struct message{
        std::string title;
        std::string text;
        int priority;
        std::string tags;
    };

    void workerLoop();
    bool post(const message& m);

    std::string topic_;
    std::string server_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<message> queue_;
    bool busy_ = false;
    bool stopping_ = false;
    size_t sent_ = 0;
    size_t failed_ = 0;
    std::thread worker_;
};
