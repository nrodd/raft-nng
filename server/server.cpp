#include <string>
#include <iostream>
#include <vector>
#include <mutex>
#include <nng/nng.h>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

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
    std::string leaderId;             // append_entries
    int prevLogIndex;                 // append_entries
    int prevLogTerm;                  // append_entries
    std::vector<std::string> entries; // append_entries
    int leaderCommit;                 // append_entries
    std::string candidateId;          // request_vote
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

struct LogEntry
{
    int termCommited;
    std::string command;
};

class Server
{
public:
    int currentTerm = 0;                // persistent
    std::string votedFor;               // persistent
    std::vector<LogEntry> logs;         // persistent log[0] -> (term_received, command)
    std::vector<std::string> neighbors; // persistent on the application level?
    int commitIndex = 0;                // volatile
    int lastApplied = 0;                // volatile

    // state stuff
    StateType state = FOLLOWER;

    // thread stuff
    bool running = false;

    bool applied = false;
    std::mutex applied_mutex;

    void send_message(nng_socket sock, RPCMessage msg)
    {
        // 1. send the message
        int data_size = sizeof(msg);
        int temp_response = nng_send(sock, &msg, data_size, 0);

        if (temp_response != 0)
        {
            // handle error
        }

        // 2. handle the response
        RPCMessageResponse response_data;
        size_t response_data_size = sizeof(response_data);

        temp_response = nng_recv(sock, &response_data, &response_data_size, 0);

        if (temp_response == -1)
        {
            // handle error
        }

        if (temp_response == sizeof(response_data))
        {
            std::cout << "Received response for term:" << response_data.term << "\n";
            // now actually process the data or whatever, prob need to return result to whatever function called it
        }
    }

    void receive_message_thread(nng_socket sock)
    {
        while (running)
        {
            RPCMessage incoming_data;
            size_t incoming_data_size = sizeof(incoming_data);

            int temp_response = nng_recv(sock, &incoming_data, &incoming_data_size, 0);

            if (temp_response == sizeof(incoming_data))
            {
                std::cout << "Incoming RPC type:" << incoming_data.type << "\n";

                // process request_vote
                RPCMessageResponse reply_data;

                if (incoming_data.type == 0)
                {
                    auto [term, truthy] = process_request_vote(incoming_data.term, incoming_data.candidateId, incoming_data.lastLogIndex, incoming_data.lastLogTerm);
                    reply_data.success = true;
                    reply_data.term = 50;
                    reply_data.voteGranted = true;
                }
                else if (incoming_data.type == 1)
                {
                    auto [term, truthy] = process_request_vote(incoming_data.term, incoming_data.candidateId, incoming_data.lastLogIndex, incoming_data.lastLogTerm);
                    reply_data.success = false; // we dont actually need this
                    reply_data.term = term;
                    reply_data.voteGranted = truthy;
                }
                // process append_entries

                int reply_data_size = sizeof(reply_data);
                nng_send(sock, &reply_data, reply_data_size, 0); // should we be sending this on socket? how do we know which to send it to?
            }
            else if (temp_response == -1)
            {
                // handle error
                std::cout << "Error: Could not process incoming response" << "\n";
                break;
            }
            else
            {
                // handle error for incorrect size
                std::cout << "Error: Incoming size does not match expectations" << "\n";
                break;
            }
        }
    }

    std::pair<int, bool> process_append_entries(int term, int leaderId, int prevLogIndex, int prevLogTerm, std::vector<LogEntry> entries, int leaderCommit) // RPC
    {
        // set our applied var to true so that the follower doesnt time out
        applied_helper_function(true);

        // do we need to process hearbeats any differently?

        // 1. Reply false if term < currentTerm
        if (term < currentTerm)
        {
            return {currentTerm, false};
        }

        // 2. Reply false if log doesn’t contain an entry at prevLogIndex
        // whose term matches prevLogTerm
        if (logs[prevLogIndex].termCommited != prevLogTerm)
        {
            return {currentTerm, false};
        }

        // 3. If an existing entry conflicts with a new one (same index
        // but different terms), delete the existing entry and all that
        // follow it
        for (int i = 0; i < entries.size(); i++)
        {
            int logIndex = prevLogIndex + 1 + i;
            if (logIndex < logs.size() && logs[logIndex].termCommited != term)
            {
                logs.erase(logs.begin() + logIndex, logs.end());
            }
        }

        // 4. Append any new entries not already in the log
        for (int i = 0; i < entries.size(); i++)
        {
            int logIndex = prevLogIndex + 1 + i;
            if (logIndex >= logs.size())
            {
                logs.push_back(entries[i]);
            }
        }

        // 5. If leaderCommit > commitIndex, set commitIndex =
        // min(leaderCommit, index of last new entry)
        if (leaderCommit > commitIndex)
        {
            int lastNewEntryIndex = prevLogIndex + (int)entries.size();
            commitIndex = std::min(leaderCommit, lastNewEntryIndex);
        }
    };

    std::pair<int, bool> process_request_vote(int term, std::string candidateId, int lastLogIndex, int lastLogTerm) // RPC
    {
        //  1. Reply false if term < currentTerm (§5.1)
        if (term < currentTerm)
        {
            return {currentTerm, false};
        }
        // if we see a higher term, update and step down to follower
        if (term > currentTerm)
        {
            currentTerm = term;
            votedFor = -1;
            state = FOLLOWER;
        }

        // 2. Check if candidate log is at least as up-to-date as ours
        int myLastLogIndex = logs.size() - 1;
        int myLastLogTerm = logs.empty() ? 0 : logs.back().termCommited;

        bool logIsUpToDate = (lastLogTerm > myLastLogTerm) ||
                             (lastLogTerm == myLastLogTerm && lastLogIndex >= myLastLogIndex);
        // 2. If votedFor is null or candidateId, and candidate’s log is at
        // least as up-to-date as receiver’s log, grant vote
        if ((votedFor.empty() || votedFor == candidateId) && logIsUpToDate)
        {
            votedFor = candidateId;
            return {currentTerm, true};
        }
    };

    void applied_helper_function(bool val)
    {
        applied_mutex.lock();
        applied = val;
        applied_mutex.unlock();
    }

    void timer_thread()
    {
        while (running)
        {
            if (state == LEADER)
            {
                // send out heartbeat ever 50ms or something
                std::chrono::milliseconds leader_timeout(50);
                std::this_thread::sleep_for(leader_timeout);

                // now i need to send append_entries call right?
            }
            else if (state == FOLLOWER)
            {
                // need access to a timer variable with a mutex that I can update
                // when receiving an RPC for append_entries

                // generating random number
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(150, 300);
                int random_number = dist(gen);
                std::chrono::milliseconds follower_timeout(random_number);

                // set applied to be false
                applied_helper_function(false);

                // starting clock
                std::this_thread::sleep_for(follower_timeout);

                // check to see if applied is false or true
                if (applied == true)
                {
                    // carry on as a follower
                }
                else
                {
                    // start an election
                }
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
