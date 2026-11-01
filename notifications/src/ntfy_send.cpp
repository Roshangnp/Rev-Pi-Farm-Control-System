#include <cstdio>
#include <cstdlib>
#include <string>

#include "ntfy.h"
#include "ntfy_config.h"

// Sends one notification through the ntfy module and waits for it to go out.
//   ntfy_send [-p priority 1-5] [-t tag1,tag2] [--topic name] [--server url] <title> <message>
// Topic and server default to the ones built in from ntfy_topic.txt.
int main(int argc, char** argv) {
    std::string topic = NTFY_TOPIC, server = NTFY_SERVER, tags, title, message;
    int priority = 3;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        bool has_value = i + 1 < argc;
        if (arg == "-p" && has_value) priority = std::atoi(argv[++i]);
        else if (arg == "-t" && has_value) tags = argv[++i];
        else if (arg == "--topic" && has_value) topic = argv[++i];
        else if (arg == "--server" && has_value) server = argv[++i];
        else if (positional == 0) {title = arg; ++positional;}
        else if (positional == 1) {message = arg; ++positional;}
        else positional = 99;
    }
    if (positional != 2) {
        std::fprintf(stderr, "usage: %s [-p priority 1-5] [-t tag1,tag2] [--topic name] [--server url] <title> <message>\n", argv[0]);
        return 2;
    }

    ntfy_notifier notifier(topic, server);
    if (!notifier.enabled()) {
        std::fprintf(stderr, "No ntfy topic: put one in ntfy_topic.txt and rebuild, or pass --topic\n");
        return 1;
    }
    notifier.send(title, message, priority, tags);
    // 3 tries of up to 10 s each, plus 5 s and 30 s between them
    notifier.flush(std::chrono::seconds(70));
    if (notifier.sentCount() != 1) return 1;
    std::printf("sent to %s/%s\n", server.c_str(), topic.c_str());
    return 0;
}
