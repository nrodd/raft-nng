#include <string>
#include <iostream>
#include <vector>
#include <mutex>
#include <nng/nng.h>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>
#include <unordered_map>

// need to update persistent state to actually write to disk

enum StateType
{
    LEADER,
    FOLLOWER,
    CANDIDATE
};

struct LogEntry
{
    int termCommited;
    std::string command;
};

// we can probaby combine(prevLogIndex, prevLogTerm) with (lastLogIndex, lastLogTerm)
struct RPCMessage
{
    int type;                           // 0 for append_entries, 1 for request_vote, 2 for client request
    int term;                           // append_entries and request_vote
    std::string leaderId;               // append_entries
    int prevLogIndex;                   // append_entries
    int prevLogTerm;                    // append_entries
    std::vector<LogEntry> entries;      // append_entries
    int leaderCommit;                   // append_entries
    std::string candidateId;            // request_vote
    int lastLogIndex;                   // request_vote
    int lastLogTerm;                    // request_vote
    std::string client_request_command; // client_request
};

// we can probably combine success and voteGranted
struct RPCMessageResponse
{
    int error;        // append_entries and request_vote
    int term;         // append_entries and request_vote
    bool success;     // append_entries
    bool voteGranted; // request_vote
};

class Server
{
public:
    int currentTerm = 0;                // persistent
    std::string votedFor = "";          // persistent
    std::vector<LogEntry> logs;         // persistent log[0] -> (term_received, command)
    std::vector<std::string> neighbors; // persistent on the application level?
    int commitIndex = -1;               // volatile
    int lastApplied = -1;               // volatile
    std::string gui_id = "";

    std::string id = "";

    // state stuff
    StateType state = FOLLOWER;

    // thread stuff
    bool running = false;

    bool applied = false;
    std::mutex applied_mutex;

    nng_socket sock;
    std::unordered_map<std::string, nng_socket> peer_sockets;

    int get_last_log_index() const
    {
        return logs.empty() ? -1 : static_cast<int>(logs.size()) - 1;
    }

    int get_last_log_term() const
    {
        return logs.empty() ? 0 : logs.back().termCommited;
    }

    RPCMessageResponse send_message(const std::string &peer, RPCMessage msg)
    {
        RPCMessageResponse response_data{};
        response_data.error = 0;

        auto it = peer_sockets.find(peer);
        if (it == peer_sockets.end())
        {
            response_data.error = NNG_ENOENT;
            return response_data;
        }

        nng_socket peer_sock = it->second;

        constexpr int kMaxRetries = 2;
        constexpr nng_duration kTimeoutMs = 100;

        // Per-call timeout (can also be set once in main)
        nng_socket_set_ms(peer_sock, NNG_OPT_SENDTIMEO, kTimeoutMs);
        nng_socket_set_ms(peer_sock, NNG_OPT_RECVTIMEO, kTimeoutMs);

        for (int attempt = 0; attempt <= kMaxRetries; ++attempt)
        {
            size_t data_size = sizeof(msg);
            int rv = nng_send(peer_sock, &msg, data_size, 0);
            if (rv != 0)
            {
                if (rv == NNG_ETIMEDOUT && attempt < kMaxRetries)
                    continue;
                response_data.error = rv;
                return response_data;
            }

            size_t response_data_size = sizeof(response_data);
            rv = nng_recv(peer_sock, &response_data, &response_data_size, 0);

            if (rv == 0 && response_data_size == sizeof(response_data))
            {
                return response_data; // success
            }

            if (rv == NNG_ETIMEDOUT && attempt < kMaxRetries)
            {
                continue; // retry same peer
            }

            response_data.error = (rv == 0) ? NNG_EINVAL : rv;
            return response_data;
        }

        response_data.error = NNG_ETIMEDOUT;
        return response_data;
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

                RPCMessageResponse reply_data;

                if (incoming_data.type == 0)
                {
                    // process append_entries
                    auto [term, truthy] = process_append_entries(incoming_data.term, incoming_data.leaderId, incoming_data.prevLogIndex, incoming_data.prevLogTerm, incoming_data.entries, incoming_data.leaderCommit);
                    reply_data.success = truthy;
                    reply_data.term = term;
                }
                else if (incoming_data.type == 1)
                {
                    // process request_vote
                    auto [term, truthy] = process_request_vote(incoming_data.term, incoming_data.candidateId, incoming_data.lastLogIndex, incoming_data.lastLogTerm);
                    reply_data.voteGranted = truthy;
                    reply_data.term = term;
                }
                else if (incoming_data.type == 2)
                {
                    // process client request
                    process_client_request(incoming_data.client_request_command);
                }

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

    void process_client_request(std::string command)
    {
        if (state == FOLLOWER)
        {
            // followers can't handle client requests directly
            // redirect client to the known leader
            std::cout << "Not the leader, redirecting to leader" << "\n";
            // you could store a currentLeaderId member variable to redirect to
        }
        else if (state == LEADER)
        {
            // 1. append entry to local log
            LogEntry newEntry;
            newEntry.termCommited = currentTerm;
            newEntry.command = command;
            logs.push_back(newEntry);

            // 2. send append_entries to all followers
            int prevLogIndex = logs.size() - 2; // index before new entry
            int prevLogTerm = prevLogIndex >= 0 ? logs[prevLogIndex].termCommited : 0;
            std::vector<LogEntry> newEntries = {newEntry};

            int successCount = 1; // count self
            for (int i = 0; i < neighbors.size(); i++)
            {
                bool success = append_entries(neighbors[i], currentTerm, id, prevLogIndex, prevLogTerm, newEntries, commitIndex);
                if (success)
                {
                    successCount++;
                }
            }

            // 3. if majority acknowledged, commit the entry
            if (successCount >= neighbors.size() / 2 + 1)
            {
                commitIndex = logs.size() - 1;
                lastApplied = commitIndex;
                std::cout << "Entry committed at index: " << commitIndex << "\n";
            }
        }
        else if (state == CANDIDATE)
        {
            // in an election, reject client requests
            std::cout << "In election, cannot process client request" << "\n";
        }
    }

    std::pair<int, bool> process_append_entries(int term, std::string leaderId, int prevLogIndex, int prevLogTerm, std::vector<LogEntry> entries, int leaderCommit) // RPC
    {
        // set our applied var to true so that the follower doesnt time out
        applied_helper_function(true);

        // do we need to process hearbeats any differently?

        // If AppendEntries RPC received from new leader: convert to follower
        if (state == CANDIDATE)
        {
            // convert to follower
            state = FOLLOWER;
        }

        // 1. Reply false if term < currentTerm
        if (term < currentTerm)
        {
            return {currentTerm, false};
        }

        // 2. Reply false if log doesn’t contain an entry at prevLogIndex
        // whose term matches prevLogTerm. If prevLogIndex == -1, this check passes.
        if (prevLogIndex >= 0)
        {
            if (prevLogIndex >= static_cast<int>(logs.size()))
            {
                return {currentTerm, false};
            }
            if (logs[prevLogIndex].termCommited != prevLogTerm)
            {
                return {currentTerm, false};
            }
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
        return {currentTerm, true};
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
            votedFor = "";
            state = FOLLOWER;
        }

        // 2. Check if candidate log is at least as up-to-date as ours
        int myLastLogIndex = get_last_log_index();
        int myLastLogTerm = get_last_log_term();

        bool logIsUpToDate = (lastLogTerm > myLastLogTerm) ||
                             (lastLogTerm == myLastLogTerm && lastLogIndex >= myLastLogIndex);
        // 2. If votedFor is null or candidateId, and candidate’s log is at
        // least as up-to-date as receiver’s log, grant vote
        if ((votedFor.empty() || votedFor == candidateId) && logIsUpToDate)
        {
            votedFor = candidateId;
            return {currentTerm, true};
        }
        return {currentTerm, false};
    };

    void applied_helper_function(bool val)
    {
        applied_mutex.lock();
        applied = val;
        applied_mutex.unlock();
    }

    /*
    invoked by leader to append logs or send hearbeat. called from client thread
    and timer thread

    args:
    int term -> leader's term
    std::string leaderId -> leader's id
    int prevLogIndex -> index of leader's last log entry
    int prevLogTerm -> term of leader's last log entry
    std::vector<LogEntry> entries -> vector of log entries to append
    int leaderCommit -> leader's commit index

    returns:
    bool -> success status for candidate


    */
    bool append_entries(const std::string &peer, int term, std::string leaderId, int prevLogIndex, int prevLogTerm, std::vector<LogEntry> entries, int leaderCommit) // RPC
    {
        std::cout << "sending append_entries" << "\n";
        RPCMessage receiver_msg;
        receiver_msg.type = 0;
        receiver_msg.term = term;
        receiver_msg.leaderId = leaderId;
        receiver_msg.prevLogIndex = prevLogIndex;
        receiver_msg.prevLogTerm = prevLogTerm;
        receiver_msg.entries = entries;
        receiver_msg.leaderCommit = leaderCommit;

        RPCMessageResponse receiver_response = send_message(peer, receiver_msg);
        return receiver_response.success;
    };

    void convert_to_leader()
    {
        state = LEADER;
        applied_helper_function(true); // so we dont timeout when were converted

        // now i need to send append_entries heartbeat call right?
        std::vector<LogEntry> empty_logs;
        int prevLogIndex = get_last_log_index();
        int prevLogTerm = get_last_log_term();
        for (int i = 0; i < neighbors.size(); i++)
        {
            append_entries(neighbors[i], currentTerm, id, prevLogIndex, prevLogTerm, empty_logs, commitIndex);
        }
    }

    /*
    invoked by candidate to gather votes

    args:
    int term -> candidates term
    std::string candidateId -> candidate requesting vote
    int lastLogIndex -> index of candidate's last log entry
    int lastLogTerm -> term of candidate's last log entry

    rets:
    int term -> currentTerm, for candidate to update itself
    bool voteGranted -> true means candidate received vote

    */
    bool request_vote(const std::string &peer, int term, std::string candidateId, int lastLogIndex, int lastLogTerm) // RPC
    {
        std::cout << "requesting vote" << "\n";
        RPCMessage receiver_msg;
        receiver_msg.type = 1;
        receiver_msg.term = term;
        receiver_msg.candidateId = id;
        receiver_msg.lastLogIndex = lastLogIndex;
        receiver_msg.lastLogTerm = lastLogTerm;
        RPCMessageResponse receiver_response = send_message(peer, receiver_msg);
        return receiver_response.voteGranted;
    };

    void start_election()
    {
        std::cout << "Start Election called" << '\n';

        state = CANDIDATE;
        votedFor = id;
        currentTerm++;
        // reset election timer

        int votedForCount = 1; // vote for self i think
        int lastLogIndex = get_last_log_index();
        int lastLogTerm = get_last_log_term();
        // will have to change this to run in parallel in future
        for (int i = 0; i < neighbors.size(); i++)
        {
            std::cout << "In loop" << '\n';

            // make request vote RPC call to each one
            std::cout << "Last Applied: " << lastApplied << '\n';
            std::cout << "Current Term: " << currentTerm << '\n';
            std::cout << "ID: " << neighbors[i] << '\n';
            std::cout << "lastLogTerm: " << lastLogTerm << '\n';

            bool resp = request_vote(neighbors[i], currentTerm, id, lastLogIndex, lastLogTerm);
            std::cout << "After request vote" << '\n';
            if (resp == true)
            {
                votedForCount++;
            }
        }
        std::cout << "Made it through loop" << '\n';

        if (votedForCount >= neighbors.size() / 2 + 1)
        {
            // this means we won the election, become leader
            convert_to_leader();
        }
        else
        {
            std::cout << "Weird Case" << '\n';
            state = FOLLOWER;
        }

        // should we be discovering a new current leader iin the other thread, or should we process that from the request vote call?
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

                // now i need to send append_entries heartbeat call right?
                std::vector<LogEntry> empty_logs;
                int prevLogIndex = get_last_log_index();
                int prevLogTerm = get_last_log_term();
                for (int i = 0; i < neighbors.size(); i++)
                {
                    append_entries(neighbors[i], currentTerm, id, prevLogIndex, prevLogTerm, empty_logs, commitIndex);
                }
            }
            else if (state == FOLLOWER || state == CANDIDATE)
            {
                // need access to a timer variable with a mutex that I can update
                // when receiving an RPC for append_entries

                // generating random number
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(150, 300);
                int random_number = dist(gen);
                std::chrono::milliseconds follower_timeout(random_number);
                std::cout << "Random Number: " << random_number << '\n';

                // set applied to be false
                applied_helper_function(false);

                // starting clock
                std::this_thread::sleep_for(follower_timeout);

                // check to see if applied is false or true
                if (applied == true)
                {
                    // carry on as a follower
                    continue;
                }
                else
                {
                    std::cout << "Leader timed-out, starting election" << "\n";
                    // start an election
                    start_election();
                    continue;
                }
            }
        }
    }
};

void fatal(const char *func, int rv)
{
    std::cerr << func << ": " << nng_strerror((nng_err)rv) << std::endl;
    exit(1);
}

int main()
{

    char *url = "tcp://192.168.1.82:5000";

    Server myServer;
    myServer.id = url;

    nng_socket sock;
    int rv;

    // Initialize NNG library
    if ((rv = nng_init(NULL)) != 0)
        fatal("nng_init", rv);

    // Open a Pair socket
    if ((rv = nng_bus0_open(&sock)) != 0)
    {
        std::cerr << "bus open failed: " << nng_strerror((nng_err)rv) << "\n";
        return 1;
    }

    nng_listener listener;
    if ((rv = nng_listener_create(&listener, sock, url)) != 0)
        fatal("nng_listener_create", rv);
    if ((rv = nng_listener_start(listener, 0)) != 0)
        fatal("nng_listener_start", rv);

    std::cout << "Socket ready at " << url << std::endl;

    // set neighbors to the other servers' addresses
    myServer.neighbors = {
        "tcp://192.168.1.82:5000",
        "tcp://192.168.1.83:5000"};
    // remove self from neighbors
    myServer.neighbors.erase(
        std::remove(myServer.neighbors.begin(), myServer.neighbors.end(), url),
        myServer.neighbors.end());

    // create a dedicated outbound socket per neighbor
    for (auto &neighbor : myServer.neighbors)
    {
        nng_socket peer_sock;
        if ((rv = nng_bus0_open(&peer_sock)) != 0)
        {
            std::cerr << "Failed to open outbound socket for " << neighbor << ": " << nng_strerror((nng_err)rv) << "\n";
            continue;
        }

        if ((rv = nng_dial(peer_sock, neighbor.c_str(), nullptr, NNG_FLAG_NONBLOCK)) != 0)
        {
            std::cerr << "Failed to dial " << neighbor << ": " << nng_strerror((nng_err)rv) << "\n";
            continue;
        }

        myServer.peer_sockets[neighbor] = peer_sock;
    }

    myServer.sock = sock;
    myServer.running = true;

    std::thread recv_thread(&Server::receive_message_thread, &myServer, sock);
    std::thread timer(&Server::timer_thread, &myServer);

    recv_thread.join();
    timer.join();

    return 0;
}