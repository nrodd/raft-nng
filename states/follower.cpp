/*
Followers are passive: they issue no requests on
their own but simply respond to requests from leaders
and candidates.

If a follower receives no communication over a period of time
called the election timeout, then it assumes there is no viable leader and begins an election to choose a new leader

To begin an election, a follower increments its current
term and transitions to candidate state

*/
