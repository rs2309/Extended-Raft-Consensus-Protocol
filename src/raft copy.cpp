// #include <iostream>
// #include <memory>

// #include "common/utils/rand_gen.hpp"
// #include "rafty/raft.hpp"

// #include <thread>
// #include <chrono>

// namespace rafty
// {

//   const char *roleToString(Role role)
//   {
//     switch (role)
//     {
//     case Role::Leader:
//       return "Leader";
//     case Role::Follower:
//       return "Follower";
//     case Role::Candidate:
//       return "Candidate";
//     default:
//       return "Unknown";
//     }
//   }
//   Raft::Raft(const Config &config, MessageQueue<ApplyResult> &ready)
//       : id(config.id),
//         listening_addr(config.addr),
//         peer_addrs(config.peer_addrs),
//         dead(false),
//         ready_queue(ready),
//         logger(utils::logger::get_logger(id))
//   // TODO: add more field if desired
//   {
//     // TODO: finish it
//     currentTerm = 0;
//     votedFor = -1;
//     commitIndex = 0;
//     lastApplied = 0;
//     log = std::vector<Entry>();

//     timerCounter = 0;
//     // resetTimer.store(false);
//   }

//   void Raft::start_timer(uint64_t currentCounter, int delayInMilliseconds, int mode)
//   {
//     {
//       std::unique_lock<std::mutex> lock(this->mtx);
//       // Wait for the specified duration or until notified
//       if (this->cv.wait_for(lock, std::chrono::milliseconds(delayInMilliseconds), [this, currentCounter]()
//                             { return this->dead.load() || this->timerCounter != currentCounter; }))
//       {
//         // Timer was reset or interrupted
//         this->logger->info("Timer {} was reset -  By TimerId {} dead {} mode {} Role {}", currentCounter, this->timerCounter, this->dead.load(), mode, roleToString(this->currentRole));

//         return;
//       }
//     }

//     // Lock is released here
//     this->logger->info("Timer expired for mode {} Role {}", mode, roleToString(this->currentRole));

//     if (this->currentRole == Role::Follower || this->currentRole == Role::Candidate)
//     {
//       // It must be running election timer
//       std::thread(&Raft::become_candidate, this).detach();
//     }
//     else
//     {
//       std::thread(&Raft::send_entries, this).detach();
//     }
//     // If we are here, it means the timer was reset or interrupted
//   }

//   Raft::~Raft() { this->stop_server(); }

//   void Raft::run()
//   {
//     // TODO: kick off the raft instance
//     // Note: this function should be non-blocking
//     active = true;
//     currentRole = Role::Follower;
//     become_follower(0);

//     // lab 1
//   }

//   void Raft::start_election_timer()
//   {
//     std::mt19937 rng(std::random_device{}());
//     // int min=electionTimeout;
//     // int max=2*electionTimeout;
//     int min = 100;
//     int max = 500;

//     std::uniform_int_distribution<> dist(min, max);
//     int timer = dist(rng);
//     logger->info("Started election timer {}: {}", timerCounter + 1, timer);
//     uint64_t currentCounter;
//     {
//       std::unique_lock<std::mutex> lock(mtx);
//       currentCounter = ++timerCounter; // Increment version to invalidate old timers
//     }
//     cv.notify_all();
//     std::thread(&Raft::start_timer, this, currentCounter, timer, 0).detach();
//   }
//   void Raft::start_heartbeat_timer()
//   {
//     logger->info("Started heartbeat timer: {}", broadcastTimeout);
//     uint64_t currentCounter;
//     {
//       std::unique_lock<std::mutex> lock(mtx);
//       currentCounter = ++timerCounter; // Increment version to invalidate old timers
//     }
//     cv.notify_all();
//     std::thread(&Raft::start_timer, this, currentCounter, broadcastTimeout, 1).detach();
//   }
//   void Raft::become_follower(int term)
//   {
//     // We should set a term here.
//     start_election_timer();
//   }
//   void Raft::become_candidate()
//   {
//     logger->info("Raft: Now becoming candidate");
//     {
//       std::unique_lock<std::mutex> lock(mtx);
//       currentRole = Role::Candidate;
//       currentTerm++;
//       votedFor = id;
//       votes.clear();
//       votes.insert(id);

//       // Assuming no timers are running at this point
//       //  resetTimer.store(true);
//     }
//     // cv.notify_all();

//     // Here there should be a mechanism to check the current status of the timer. Ideally, it should not be running.
//     std::thread(&Raft::start_election_timer, this).detach();
//     request_votes(currentTerm);
//   }
//   void Raft::handle_pending_sync_props()
//   {
//     std::unique_lock<std::mutex> lock(this->mtx);
//     if (proposal_cvs.size() > 0)
//       logger->info("Clearing Sync Proposals: {}", (int)proposal_cvs.size());

//     for (auto &kv : proposal_cvs)
//     {
//       kv.second->notify_one();
//     }
//     proposal_cvs.clear();
//   }

//   void Raft::become_leader()
//   {
//     {
//       std::unique_lock<std::mutex> lock(mtx);
//       currentRole = Role::Leader;
//       // Assuming the timer is Running
//       //  resetTimer.store(true);

//       // Instantiate data structures for the leader
//       nextIndex.clear();
//       matchIndex.clear();
//       for (const auto &m : peers_)
//       {
//         auto target_id = m.first;
//         nextIndex[target_id] = (uint64_t)log.size() + 1;
//         matchIndex[target_id] = 0;
//       }
//     }
//     logger->info("Selected as leader Term {} status {}", currentTerm, roleToString(currentRole));
//     // Send Empty Append RPC First, Then start heartbeat timer ---- CHECK THIS
//     std::thread(&Raft::start_heartbeat_timer, this).detach();
//     // Rcv.notify_all();//
//   }
//   void Raft::request_votes(uint64_t election_term)
//   {
//     logger->info("Raft: Initiating Election");

//     for (const auto &m : peers_)
//     {
//       auto target_id = m.first;
//       // check this : synchronous or async
//       std::thread([this, target_id, election_term]()
//                   {
//         auto context = this->create_context(target_id);
//         raftpb::RequestVoteRequest req;
//         req.set_term(this->currentTerm);
//         req.set_candidate_id(this->id);
//         req.set_last_log_term(this->log.size() > 0 ? this->log.back().term : 0); // dummy
//         req.set_last_log_index(this->log.size() > 0 ? this->log.size() : 0); // dummy

//         raftpb::RequestVoteReply reply;
//         grpc::Status status = this->peers_[target_id]->RequestVote(&*context, req, &reply);

//         if (status.ok()) {
//             // Process response
//             logger->info("Request Vote Reply: {} target_id {} currentTerm {} electionTerm {} Role {}", reply.vote_granted(), target_id,this->currentTerm,election_term,roleToString(this->currentRole));

//             if(!reply.vote_granted()){
//               if (reply.term() > this->currentTerm)
//               {
//                 std::unique_lock<std::mutex> lock(mtx);
//                 currentTerm = reply.term();
//                 votedFor = -1; // reset the vote //Here we dont know who is the leader. But someone is elected as leader.
//                 // step down if leader
//                 if(currentRole == Role::Leader)
//                     handle_pending_sync_props();

//                 currentRole = Role::Follower;
//                 std::thread(&Raft::start_election_timer,this).detach();

//                 logger->info("Received Higher Term in Vote Reply {}", currentTerm);
//               }
//             }
//             if(this->currentTerm==election_term && this->currentRole==Role::Candidate){

//               if(reply.vote_granted()){
//                 this->votes.insert(target_id);
//                 // this->logger->info("Current Votes: {} Majority: {}", (std::uint32_t)this->votes.size(),(std::uint32_t)this->peers_.size());
//                 if(this->votes.size()>this->peers_.size()/2)
//                   this->become_leader();
//               }
//             }
//         } else {
//             logger->info("ReqVote RPC Failed to parse text. target_id: {} ", target_id);
//         } })
//           .detach();
//     }
//   }

//   raftpb::AppendEntriesReply *Raft::append_entries(const raftpb::AppendEntriesRequest *request, raftpb::AppendEntriesReply *reply)
//   {
//     if (!active)
//     {
//       return reply;
//     }
//     std::unique_lock<std::mutex> lock(mtx);

//     if (request->term() < currentTerm)
//     {
//       reply->set_term(currentTerm);
//       reply->set_success(false);
//       return reply;
//     }

//     logger->info("Recieved Entries from Leader {}", request->leader_id());
//     {
//       // updating term and starting election timer
//       // ideally become follower should be called
//       if (request->term() > currentTerm)
//       {
//         currentTerm = request->term();
//         votedFor = -1; // reset the vote

//         logger->info("AE Term {}", currentTerm);
//       }

//       if (currentRole == Role::Leader)
//         handle_pending_sync_props();

//       currentRole = Role::Follower;
//       // Assuming either election timer or heartbeat timer is running
//       std::thread(&Raft::start_election_timer, this).detach();
//       // Rcv.notify_all();
//     }

//     if (request->prev_log_index() > 0 && ((request->prev_log_index() - 1 < log.size() && log[request->prev_log_index() - 1].term != request->prev_log_term()) || (request->prev_log_index() - 1 >= log.size())))
//     {
//       reply->set_term(currentTerm);
//       reply->set_success(false);

//       if (request->prev_log_index() - 1 < log.size() && request->prev_log_index() - 1 >= 0)
//       {
//         logger->info("Prev Log Index {} Term {} Command {}", request->prev_log_index() - 1, this->log[request->prev_log_index() - 1].term, this->log[request->prev_log_index() - 1].command);
//       }
//       else
//       {
//         logger->info("Prev Log Index {}", request->prev_log_index() - 1);
//       }

//       // reply->set_required_term(request->prev_log_index() - 1 < this->log.size() ? this->log[request->prev_log_index() - 1].term : 0);
//       auto conflict_info = reply->mutable_conflict_info();

//       if (request->prev_log_index() - 1 >= log.size())
//       {
//         // Follower's log is shorter than prevLogIndex
//         conflict_info->set_conflict_index(log.size() + 1);
//         conflict_info->set_conflict_term(0);
//       }
//       else
//       {
//         // Follower has an entry at prevLogIndex - 1, but term does not match
//         uint64_t conflict_term = log[request->prev_log_index() - 1].term;
//         conflict_info->set_conflict_term(conflict_term);

//         // Find the first index where this conflicting term appears
//         uint64_t conflict_index = request->prev_log_index();
//         for (int64_t i = request->prev_log_index() - 1; i >= 0; --i)
//         {
//           if (log[i].term != conflict_term)
//           {
//             conflict_index = i + 2; // Indices are 1-based
//             break;
//           }
//           if (i == 0)
//           {
//             conflict_index = 1;
//             break;
//           }
//         }
//         conflict_info->set_conflict_index(conflict_index);
//       }

//       return reply;
//     }
//     // At this point setting success to true ???
//     reply->set_term(currentTerm);
//     reply->set_success(true);
//     if (request->entries_size() > 0 && (request->prev_log_index() >= log.size() || (request->prev_log_index() < log.size() && log[request->prev_log_index()].term != request->entries(0).term())))
//     {
//       logger->info("Inside if ---- ");
//       if (request->prev_log_index() < log.size() && log[request->prev_log_index()].term != request->entries(0).term())
//       {
//         log.erase(log.begin() + request->prev_log_index(), log.end());
//         logger->info("Erased logs from index {}", request->prev_log_index());
//       }
//       for (int i = 0; i < request->entries_size(); ++i)
//       {
//         const raftpb::Entry &e = request->entries(i);
//         Entry new_entry;
//         new_entry.term = e.term();
//         new_entry.command = e.command();
//         this->log.push_back(new_entry);
//         logger->info("Added log {} at index {} term {}", new_entry.command, this->log.size(), new_entry.term);
//       }
//     }
//     // logger->info("leader {} current {}",request->leader_commit(), commitIndex )
//     if (request->leader_commit() > commitIndex)
//     {
//       uint64_t last_new_entry_index = this->log.size(); // Index of last new entry
//       commitIndex = std::min((uint64_t)request->leader_commit(), last_new_entry_index);
//       apply_entries();
//     }

//     // hoping that current term will be atleast request's term
//     return reply;
//   }

//   raftpb::RequestVoteReply *Raft::reply_vote(const raftpb::RequestVoteRequest *request, raftpb::RequestVoteReply *reply)
//   {
//     // Early return if node is inactive

//     if (!active)
//     {
//       reply->set_vote_granted(false);
//       return reply;
//     }

//     logger->info("Received Vote Request from Candidate {}", request->candidate_id());

//     std::unique_lock<std::mutex> lock(mtx); // Lock mutex for thread safety

//     // If the candidate's term is less than current term, deny the vote
//     if (request->term() < currentTerm)
//     {
//       reply->set_term(currentTerm);
//       reply->set_vote_granted(false);
//       return reply;
//     }

//     // If the candidate's term is greater, update term and convert to follower
//     bool flag = true;
//     if (request->term() > currentTerm)
//     {
//       currentTerm = request->term();
//       votedFor = -1; // Reset vote

//       if (currentRole == Role::Leader)
//         handle_pending_sync_props();

//       currentRole = Role::Follower; // Step down to follower
//       logger->info("Updated term to {} and stepped down to Follower", currentTerm);
//       flag = false;
//       std::thread(&Raft::start_election_timer, this).detach(); // Reset election timer
//     }

//     // Set reply term to current term
//     reply->set_term(currentTerm);

//     // Check if we have already voted for someone else in this term
//     if (votedFor != -1 && votedFor != request->candidate_id())
//     {
//       reply->set_vote_granted(false);
//       logger->info("Denied vote to Candidate {}: already voted for {}", request->candidate_id(), votedFor);
//       return reply;
//     }

//     // Determine if the candidate's log is at least as up-to-date as ours
//     bool candidateUpToDate = false;
//     uint64_t lastLogTerm = 0;
//     uint64_t lastLogIndex = 0;

//     if (!log.empty())
//     {
//       lastLogTerm = log.back().term;
//       lastLogIndex = log.size(); // Assuming log indices start at 1
//     }

//     uint64_t candidateLastLogTerm = request->last_log_term();
//     uint64_t candidateLastLogIndex = request->last_log_index();

//     if (candidateLastLogTerm > lastLogTerm)
//     {
//       candidateUpToDate = true;
//     }
//     else if (candidateLastLogTerm == lastLogTerm && candidateLastLogIndex >= lastLogIndex)
//     {
//       candidateUpToDate = true;
//     }
//     else
//     {
//       candidateUpToDate = false;
//     }

//     // Decide whether to grant the vote
//     if (candidateUpToDate)
//     {
//       // Grant the vote
//       reply->set_vote_granted(true);
//       votedFor = request->candidate_id();

//       if (currentRole == Role::Leader) // Not Needed
//         handle_pending_sync_props();   // Not Needed
//       currentRole = Role::Follower;    // Not Needed
//       if (flag)
//         std::thread(&Raft::start_election_timer, this).detach(); // Reset election timer
//       logger->info("Granted vote to Candidate {}", request->candidate_id());
//     }
//     else
//     {
//       // Deny the vote
//       reply->set_vote_granted(false);
//       logger->info("Denied vote to Candidate {}: candidate's log is not up-to-date", request->candidate_id());
//     }

//     return reply;
//   }
//   void Raft::apply_entries()
//   {
//     if (this->lastApplied < this->commitIndex)
//     {
//       for (int i = this->lastApplied + 1; i <= this->commitIndex; i++)
//       {
//         ApplyResult ar;
//         ar.data = this->log[i - 1].command;
//         ar.index = i;
//         ar.valid = true;
//         this->apply(ar);
//         this->lastApplied++;
//         logger->info("Applied log entry {} term {} index {}", ar.data, this->log[i - 1].term, ar.index);

//         // Notify the condition variable for this proposalIndex, if any
//         auto it = proposal_cvs.find(i);
//         if (it != proposal_cvs.end())
//         {
//           it->second->notify_one();
//         }
//       }
//     }
//   }

//   void Raft::send_entries()
//   {

//     logger->info("Appending Entries");
//     for (const auto &m : peers_)
//     {
//       auto target_id = m.first;
//       // check this : synchronous or async
//       std::thread([this, target_id]()
//                   {
//         this->logger->info("Sending Entries target {} nextIndex {} matchIndex {} logSize {}",target_id,this->nextIndex[target_id],this->matchIndex[target_id],this->log.size());
//         auto context = this->create_context(target_id);
//         raftpb::AppendEntriesRequest req;
//         req.set_term(this->currentTerm);
//         req.set_leader_id(this->id);
//         req.set_prev_log_term(this->nextIndex[target_id] > 1 ? this->log[this->nextIndex[target_id]-2].term : 0); // 0 indexing
//         req.set_prev_log_index(this->nextIndex[target_id] > 1 ? this->nextIndex[target_id]-1 : 0); // dummy
//         req.set_leader_commit(this->commitIndex);
//         //Assuming log size only increases as long as node is the leader
//         //if it steps down all reply handlers should take care of it.
//         uint64_t currentLogSize=this->log.size();
//         uint64_t entryCounter=0;
//         for(int i=this->nextIndex[target_id]-1; i<currentLogSize;i++){
//           raftpb::Entry* entry = req.add_entries();
//           entry->set_term(this->log[i].term);
//           entry->set_command(this->log[i].command);
//           entryCounter++;
//           if(entryCounter==100) //ONLY APPENDING 100 Entries at a time
//             break;
//         }

//         raftpb::AppendEntriesReply reply;
//         grpc::Status status = this->peers_[target_id]->AppendEntries(&*context, req, &reply);

//         if (status.ok()) {
//             // Process response
//             logger->info("Append Entries Reply: {}  Role {}", reply.success(),roleToString(this->currentRole));

//               //Reply Handlers -  See the comment above
//               //it target has higher term
//               if (reply.term() > this->currentTerm)
//               {
//                 std::unique_lock<std::mutex> lock(this->mtx);
//                 currentTerm = reply.term();
//                 votedFor = -1; // reset the vote //Here we dont know who is the leader. But someone is elected as leader.
//                 // step down if leader

//                 if(currentRole== Role::Leader)
//                     handle_pending_sync_props();
//                 currentRole = Role::Follower;
//                 // cv.notify_all();
//                 std::thread(&Raft::start_election_timer,this).detach();

//                 logger->info("Term {}", currentTerm);
//               }
//               else if(!reply.success()){
//                 //start from prev index - 0th
//                 //moving next index
//                 //Handle role change here.

//                 ////
//                 std::unique_lock<std::mutex> lock(this->mtx);
//                 if(this->currentRole!=Role::Leader || req.term()!=this->currentTerm) //also verifying term to handle outdated reply
//                   return;

//                 // this->nextIndex[target_id]--;
//                 // if(reply.required_term()==0)
//                 //   return;
//                 // logger->info("Append Entry Required Term {}",reply.required_term());
//                 // while(this->nextIndex[target_id]-2>=0 &&this->nextIndex[target_id]-2<currentLogSize
//                 //     && this->log[this->nextIndex[target_id]-2].term!=reply.required_term()){

//                 //     this->nextIndex[target_id]--;
//                 // }
//                 //
//                 if (reply.has_conflict_info()) {
//                   auto conflict_info = reply.conflict_info();
//                   uint64_t conflict_term = conflict_info.conflict_term();
//                   uint64_t conflict_index = conflict_info.conflict_index();

//                   // Define the search range
//                   int64_t left = static_cast<int64_t>(matchIndex[target_id]);
//                   int64_t right = static_cast<int64_t>(nextIndex[target_id]) - 2;

//                   left = std::max(int64_t(0), left);
//                   right = std::min(static_cast<int64_t>(this->log.size()) - 1, right);

//                   int64_t lastIndexWithConflictTerm = -1;
//                   while (left <= right)
//                   {
//                       int64_t mid = left + (right - left) / 2;
//                       if (this->log[mid].term == conflict_term)
//                       {
//                           lastIndexWithConflictTerm = mid;
//                           left = mid + 1; // Look for later occurrence
//                       }
//                       else if (this->log[mid].term < conflict_term)
//                       {
//                           left = mid + 1;
//                       }
//                       else
//                       {
//                           right = mid - 1;
//                       }
//                   }
//                   if (lastIndexWithConflictTerm != -1) {
//                       this->nextIndex[target_id] = lastIndexWithConflictTerm + 2; // Indices are 1-based
//                   } else {
//                       this->nextIndex[target_id] = conflict_index;
//                   }
//               } else {
//                   // If no conflict_term provided, decrement nextIndex by 1
//                   uint64_t min_next_index = this->matchIndex[target_id] + 1;
//                   if (this->nextIndex[target_id] > min_next_index) {
//                       this->nextIndex[target_id]--;
//                   } else {
//                       // Cannot decrement nextIndex[target_id] without violating the invariant
//                       logger->warn("Cannot decrement nextIndex[{}] below matchIndex[{}] + 1 (value: {})", target_id, target_id, min_next_index);
//                       // Optionally, you may need to send a snapshot or take other recovery actions
//                   }
//               }

//               }else if(reply.success()){
//                 //Assuming entire log is replicated in the recepient -- Risky
//                 // std::unique_lock<std::mutex> lock(this->mtx);

//                 // //CASE  A
//                 // if(this->currentRole!=Role::Leader || req.term()!=this->currentTerm) //also verifying term to handle outdated reply
//                 //   return;
//                 // this->matchIndex[target_id]=currentLogSize; //Verify this
//                 // this->nextIndex[target_id]=currentLogSize+1; //Verify

//                 // //CASE B
//                 // // if(this->currentRole==Role::Leader && req.term()==this->currentTerm){
//                 // //   this->matchIndex[target_id]=currentLogSize; //Verify this
//                 // //   this->nextIndex[target_id]=currentLogSize+1; //Verify
//                 // // }

//                 // if(currentLogSize<=this->log.size() && currentLogSize>this->commitIndex && this->log[currentLogSize-1].term==req.term()){
//                 //   //Checking if current log size can affect the commit Index

//                 //   std::vector<uint64_t> sorted_indexes;
//                 //   for (const auto &m : peers_)
//                 //   {
//                 //     auto target_id = m.first;
//                 //     sorted_indexes.push_back(this->matchIndex[target_id]);
//                 //   }

//                 //   sorted_indexes.push_back(currentLogSize);
//                 //   std::sort(sorted_indexes.begin(), sorted_indexes.end(), std::greater<int>());
//                 //   // Calculate quorum size

//                 //   uint64_t quorum_size = (sorted_indexes.size() / 2) + 1;

//                 //   this->commitIndex=std::max(this->commitIndex,sorted_indexes[quorum_size - 1]);
//                 //   this->logger->info("Leader Commited Index {}", this->commitIndex);
//                 //   this->apply_entries();
//                 // }

//                 std::unique_lock<std::mutex> lock(this->mtx);

//                 if (this->currentRole != Role::Leader || req.term() != this->currentTerm)
//                     return;

//                 // Update matchIndex and nextIndex
//                 uint64_t lastLogIndexSent = this->nextIndex[target_id] + req.entries_size() - 1;
//                 this->matchIndex[target_id] = lastLogIndexSent;
//                 this->nextIndex[target_id] = lastLogIndexSent + 1;

//                 // Try to advance commitIndex
//                 uint64_t newCommitIndex = this->commitIndex;
//                 uint64_t maxIndex = lastLogIndexSent;

//                 for (uint64_t N = maxIndex; N > this->commitIndex; N--)
//                 {
//                     if (this->log[N - 1].term == this->currentTerm)
//                     {
//                         int count = 1; // Count leader itself
//                         for (const auto &peer : peers_)
//                         {
//                             if (this->matchIndex[peer.first] >= N)
//                                 count++;
//                         }

//                         if (count > (peers_.size() + 1) / 2)
//                         {
//                             newCommitIndex = N;
//                             break; // Found the highest N that can be committed
//                         }
//                     }
//                 }

//                 // Update commitIndex if it has advanced
//                 if (newCommitIndex > this->commitIndex)
//                 {
//                     this->commitIndex = newCommitIndex;
//                     this->logger->info("Leader committed index {}", this->commitIndex);
//                     this->apply_entries();
//                     // No need to call cv.notify_all(); apply_entries handles notifications
//                 }
//               }
//         } else {
//             logger->info("AppendEntries RPC Failed to parse text. target_id: {} ", target_id);
//         } })
//           .detach();
//     }

//     // Assuming leader continues to send heartbeat until the status changes
//     std::thread(&Raft::start_heartbeat_timer, this).detach();
//   }

//   State Raft::get_state() const
//   {
//     // TODO: finish it
//     // lab 1
//     State s;
//     s.term = currentTerm;
//     s.is_leader = currentRole == Role::Leader ? true : false;
//     // logger->info("Current Status: Term: {} Leader: {} ", currentTerm, currentRole == Role::Leader);
//     return s;
//   }

//   ProposalResult Raft::propose(const std::string &data)
//   {
//     // TODO: finish it
//     // lab 2
//     std::unique_lock<std::mutex> lock(mtx);
//     ProposalResult res;
//     if (currentRole == Role::Leader)
//     {
//       res.is_leader = true;

//       Entry e;
//       e.term = currentTerm;
//       e.command = data;
//       log.push_back(e);

//       res.term = currentTerm;
//       res.index = log.size();

//       logger->info("Recieved a new proposal {}, Inserted at index {} term {}", data, res.index, res.term);
//     }
//     else
//     {
//       res.is_leader = false;
//     }
//     return res;
//   }

//   ProposalResult Raft::propose_sync(const std::string &data)
//   {
//     std::unique_lock<std::mutex> lock(mtx);
//     ProposalResult res;
//     if (currentRole == Role::Leader)
//     {
//       res.is_leader = true;

//       Entry e;
//       e.term = currentTerm;
//       e.command = data;
//       log.push_back(e);

//       res.term = currentTerm;
//       res.index = log.size();

//       logger->info("Received a new SYNC proposal '{}', inserted at index {} term {}", data, res.index, res.term);

//       uint64_t proposalIndex = res.index;

//       // Create a condition variable for this proposalIndex
//       auto proposal_cv = std::make_shared<std::condition_variable>();
//       proposal_cvs[proposalIndex] = proposal_cv;

//       // Now wait until commitIndex >= proposalIndex or role changes
//       proposal_cv->wait(lock, [this, proposalIndex]()
//                         { return commitIndex >= proposalIndex || currentRole != Role::Leader; });

//       // Remove the condition variable from the map
//       proposal_cvs.erase(proposalIndex);

//       if (commitIndex >= proposalIndex)
//       {
//         // Command is committed
//         return res;
//       }
//       else
//       {
//         // Role changed, no longer leader
//         res.is_leader = false;
//         return res;
//       }
//     }
//     else
//     {
//       res.is_leader = false;
//       return res;
//     }
//   }

// } // namespace rafty
