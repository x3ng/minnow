#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>

#include <deque>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), outstanding_segments_()
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  struct OutstandingSegment
  {
    TCPSenderMessage msg;
    uint64_t start_seqno;
    uint64_t rto_remain;
    uint64_t resend_cnt;
    bool zero_window = false;

    OutstandingSegment( TCPSenderMessage smsg,
                        uint64_t start_no,
                        uint64_t rto_time,
                        uint64_t rs_cnt,
                        bool window_zero = false )
      : msg( std::move( smsg ) )
      , start_seqno( start_no )
      , rto_remain( rto_time )
      , resend_cnt( rs_cnt )
      , zero_window( window_zero )
    {}
    OutstandingSegment( OutstandingSegment&& ) = default;
    OutstandingSegment& operator=( OutstandingSegment&& ) = default;
  };

  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  bool syn_sent_ = false;
  bool fin_set_ = false;
  uint64_t cr_cnt_ = 0;
  uint64_t cur_rto_ = 0;
  uint64_t last_ackno_ = 0;
  uint64_t last_seqno_ = 0;
  uint64_t window_size_ = 0;
  std::deque<OutstandingSegment> outstanding_segments_;
};
