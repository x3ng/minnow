#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  debug( "insert({}, {}, {}) called", first_index, data, is_last_substring );
  if ( is_last_substring ) {
    is_last_received_ = true;
    last_index_ = first_index + data.size();
  }
  const uint64_t available_cap = output_.writer().available_capacity();
  const uint64_t max_write_index = next_write_index_ + available_cap;
  const uint64_t start_index = max( first_index, next_write_index_ );
  const uint64_t end_index = min( first_index + data.size(), max_write_index );

  if ( start_index <= end_index && !data.empty() ) {
    const uint64_t data_start = start_index - first_index;
    const uint64_t data_len = end_index - start_index;
    string valid_data = data.substr( data_start, data_len );
    uint64_t block_start = start_index;
    auto it = pending_.upper_bound( block_start );
    if ( it != pending_.begin() ) {
      auto pre = prev( it );
      const uint64_t pre_end = pre->first + pre->second.size();
      if ( block_start <= pre_end ) {
        const uint64_t cur_str_start = pre_end - block_start;
        if ( cur_str_start < valid_data.size() ) {
          valid_data = pre->second + valid_data.substr( pre_end - block_start );
        } else {
          valid_data = pre->second;
        }
        block_start = pre->first;
        pending_.erase( pre );
      }
    }
    while ( it != pending_.end() && it->first <= block_start + valid_data.size() ) {
      const uint64_t block_end = block_start + valid_data.size();
      const uint64_t nxt_str_start = block_end - it->first;
      if ( nxt_str_start < it->second.size() ) {
        valid_data += it->second.substr( block_end - it->first );
      }
      pending_.erase( it++ );
    }
    if ( !valid_data.empty() ) {
      pending_[block_start] = valid_data;
    }
    string to_write;
    while ( !pending_.empty() && pending_.begin()->first == next_write_index_ ) {
      to_write += pending_.begin()->second;
      next_write_index_ += pending_.begin()->second.size();
      pending_.erase( pending_.begin() );
    }
    if ( !to_write.empty() ) {
      output_.writer().push( to_write );
    }
  }
  if ( is_last_received_ && next_write_index_ >= last_index_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  debug( "count_bytes_pending() called" );
  uint64_t total = 0;
  for ( const auto& [_, block] : pending_ ) {
    total += block.size();
  }
  return total;
}
