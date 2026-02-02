#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  debug( "sequence_numbers_in_flight() called" );
  uint64_t total_num = 0;
  for ( const auto& seg : outstanding_segments_ ) {
    total_num += seg.msg.sequence_length();
  }
  return total_num;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  debug( "consecutive_retransmissions() called" );
  return cr_cnt_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  debug( "push() called" );
  if ( input_.has_error() ) {
    TCPSenderMessage tsm;
    tsm.seqno = Wrap32::wrap( last_seqno_, isn_ );
    tsm.RST = true;
    transmit( tsm );
    return;
  }
  uint64_t readable_bytes = reader().bytes_buffered();
  const bool zero_window = ( window_size_ == 0 );
  const uint64_t window_right = last_ackno_ + ( zero_window ? 1 : window_size_ );
  while ( last_seqno_ < window_right
          && ( !syn_sent_ || ( !fin_set_ && reader().is_finished() ) || readable_bytes ) ) {
    const uint64_t sendable_bytes = std::min( readable_bytes, TCPConfig::MAX_PAYLOAD_SIZE );
    const uint64_t window_remain = window_right - last_seqno_;

    TCPSenderMessage tsm;
    tsm.SYN = !syn_sent_;
    uint64_t valid_payload = std::min( window_remain - !syn_sent_, sendable_bytes );
    tsm.seqno = Wrap32::wrap( last_seqno_, isn_ );
    std::string_view peek_string = reader().peek();
    tsm.payload = peek_string.substr( 0, valid_payload );
    reader().pop( valid_payload );
    readable_bytes = reader().bytes_buffered();
    const bool send_fin
      = !fin_set_ && reader().is_finished() && last_seqno_ + valid_payload + !syn_sent_ < window_right;
    if ( send_fin ) {
      tsm.FIN = true;
      fin_set_ = true;
    }
    transmit( tsm );
    outstanding_segments_.emplace_back(
      std::move( tsm ), last_seqno_, initial_RTO_ms_, 0, zero_window && syn_sent_ );
    last_seqno_ += valid_payload + send_fin + !syn_sent_;
    syn_sent_ = true;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  debug( "make_empty_message() called" );
  TCPSenderMessage tsm;
  tsm.RST = input_.has_error();
  tsm.seqno = Wrap32::wrap( last_seqno_, isn_ );
  return tsm;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  debug( "receive() called" );
  if ( msg.RST ) {
    input_.set_error();
    outstanding_segments_.clear();
    cr_cnt_ = 0;
    window_size_ = 0;
    return;
  }
  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return;
  }
  const uint64_t rec_seqno = msg.ackno->unwrap( isn_, last_seqno_ );
  if ( rec_seqno > last_seqno_ || rec_seqno < last_ackno_ ) {
    return;
  }
  last_ackno_ = rec_seqno;
  while ( !outstanding_segments_.empty() ) {
    auto& seg = outstanding_segments_.front();
    if ( last_ackno_ >= seg.start_seqno + seg.msg.sequence_length() ) {
      outstanding_segments_.pop_front();
      cr_cnt_ = 0;
      cur_rto_ = 0;
    } else {
      break;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "tick({}, ...) called", ms_since_last_tick );
  if ( !outstanding_segments_.empty() ) {
    cur_rto_ += ms_since_last_tick;
    auto& seg = outstanding_segments_.front();
    if ( seg.rto_remain <= cur_rto_ ) {
      seg.resend_cnt += 1;
      if ( !seg.zero_window ) {
        seg.rto_remain = initial_RTO_ms_ << seg.resend_cnt;
      }
      transmit( seg.msg );
      cr_cnt_ += 1;
      cur_rto_ = 0;
    }
  } else {
    cur_rto_ = 0;
  }
  push( transmit );
}
