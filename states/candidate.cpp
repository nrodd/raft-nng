#include <string>
#include <iostream>

/*
candidate is used to elect a new leader

when transitioning from follower to candidate, It votes for
itself and issues RequestVote RPCs in parallel to each of
the other servers in the cluster. A candidate continues in
this state until one of three things happens: (a) it wins the
election, (b) another server establishes itself as leader, or
(c) a period of time goes by with no winner.

Raft uses the voting process to prevent a candidate from
winning an election unless its log contains all committed
entries. A candidate must contact a majority of the cluster
in order to be elected, which means that every committed
entry must be present in at least one of those servers. If the
candidate’s log is at least as up-to-date as any other log
in that majority (where “up-to-date” is defined precisely
below), then it will hold all the committed entries
*/

class Candidate
{
public:
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
    void request_vote(int term, std::string candidateId, int lastLogIndex, int lastLogTerm) // RPC
    {
        std::cout << "requesting vote" << "\n";
        // return int term, bool voteGranted
    };
};