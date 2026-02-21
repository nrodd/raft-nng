#include <string>
#include <iostream>
#include <vector>
#include <mutex>
#include <nng/nng.h>

// need to update persistent state to actually write to disk

/*
what does server need to do:

- listen for incoming messages
- send outbound messages (hmm, or should the state classes invoke these?)
*/

enum StateType
{
    LEADER,
    FOLLOWER,
    CANDIDATE
};

// we can probaby combine(prevLogIndex, prevLogTerm) with (lastLogIndex, lastLogTerm)
struct RPCMessage
{
    int type;                         // 0 for append_entries, 1 for request_vote
    int term;                         // append_entries and request_vote
    int leaderId;                     // append_entries
    int prevLogIndex;                 // append_entries
    int prevLogTerm;                  // append_entries
    std::vector<std::string> entries; // append_entries
    int leaderCommit;                 // append_entries
    int candidateId;                  // request_vote
    int lastLogIndex;                 // request_vote
    int lastLogTerm;                  // request_vote
};

// we can probably combine success and voteGranted
struct RPCMessageResponse
{
    int term;         // append_entries and request_vote
    bool success;     // append_entries
    bool voteGranted; // request_vote
};

class Server
{
public:
    int currentTerm = 0;                // persistent
    std::string votedFor;               // persistent
    std::vector<std::string> log;       // persistent log[0] -> (command, term received)
    std::vector<std::string> neighbors; // persistent on the application level?
    int commitIndex = 0;                // volatile
    int lastApplied = 0;                // volatile

    // state stuff
    StateType state = FOLLOWER;

    void send_message()
    {
        nng_socket s;
        int nng_req0_open(nng_socket * s);
    }

    void send_message_reply()
    {
    }

    void receive_message_thread()
    {
        while (true)
        {
            nng_socket s;
            int nng_rep0_open(nng_socket * s);
        }
    }

    void process_append_entries(int term, std::string leaderId, int prevLogIndex, int prevLogTerm, std::vector<std::string> entries, int leaderCommit) // RPC
    {
        // 1. Reply false if term < currentTerm (§5.1)
        if (term < currentTerm)
        {
            // need to reply false
        }

        // 2. Reply false if log doesn’t contain an entry at prevLogIndex
        // whose term matches prevLogTerm (§5.3)
        if (log[prevLogIndex].term != prevLogTerm)
        {
            // reply false
        }

        // 3. If an existing entry conflicts with a new one (same index
        // but different terms), delete the existing entry and all that
        // follow it (§5.3)

        // 4. Append any new entries not already in the log

        // 5. If leaderCommit > commitIndex, set commitIndex =
        // min(leaderCommit, index of last new entry)
    };

    void process_request_vote(int term, std::string candidateId, int lastLogIndex, int lastLogTerm) // RPC
    {
        //  1. Reply false if term < currentTerm (§5.1)
        if (term < currentTerm)
        {
            send_vote_response(false);
            return;
        }
        // if we see a higher term, update and step down to follower
        if (term > currentTerm)
        {
            currentTerm = term;
            votedFor = "";
            state = FOLLOWER;
        }

        // 2. Check if candidate log is at least as up-to-date as ours
        int myLastLogIndex = log.size() - 1;
        int myLastLogTerm = log.empty() ? 0 : log.back().term;

        bool logIsUpToDate = (lastLogTerm > myLastLogTerm) ||
                             (lastLogTerm == myLastLogTerm && lastLogIndex >= myLastLogIndex);
        // 2. If votedFor is null or candidateId, and candidate’s log is at
        // least as up-to-date as receiver’s log, grant vote (§5.2, §5.4)
        if ((votedFor.empty() || votedFor == candidateId) && (commitIndex >= lastLogIndex && currentTerm >= lastLogTerm))
        {
            votedFor = candidateId;
            send_vote_response(true);
            return;
        }
    };

    void timer_thread()
    {
        while (running)
        {
            if (state == LEADER)
            {
                // send out heartbeat ever 50ms or something
            }
            else if (state == FOLLOWER)
            {
                // need access to a timer variable with a mutex that I can update
                // when receiving an RPC for append_entries
            }
        }
    }
};

int main()
{
    Server myServer;
    myServer.votedFor = "yeet";
    std::cout << myServer.votedFor << "\n";
    myServer.sendMessage();
    myServer.receiveMessage();
    myServer.sendMessage();
    return 0;
}
