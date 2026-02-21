#include <string>
#include <iostream>

void process_message(std::string message)
{
    // handle actual diff types of message processing here
    // if message.type == client request -> call process_client_request

    // receiver implementation for RequestVote RPC
    // receiver implementation for AppendEntries RPC
    std::cout << message << "\n";
};

void process_client_request(std::string state)
{
    if (state == "follower")
    {
        // need to redirect to current leader
    }
    else if (state == "leader")
    {
        // do i invoke objec.state.leader.process_client_request ?
    }
};

void process_append_entries() {

};

void process_request_vote()
{
}