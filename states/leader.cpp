#include <string>
#include <iostream>
#include <vector>

/*
The leader handles all client requests (if
a client contacts a follower, the follower redirects it to the
leader).

should next index and match index just be arrays, since ttheir length
can be hardcoded to the number of servers?

Once a leader has been elected, it begins servicing
client requests. Each client request contains a command to
be executed by the replicated state machines. The leader
appends the command to its log as a new entry, then issues AppendEntries RPCs in parallel to each of the other
servers to replicate the entry.

The leader decides when it is safe to apply a log entry to the state machines; such an entry is called committed.
*/

class Leader
{
public:
    std::vector<std::string> nextIndex;  // volatile
    std::vector<std::string> matchIndex; // volatile

    void handle_client_request()
    {
    }

    void append_entries(int term, std::string leaderId, int prevLogIndex, int prevLogTerm, std::vector<std::string> entries, int leaderCommit) // RPC
    {
        std::cout << "sending heartbeat" << "\n";

        // return int term, bool success
    };
};