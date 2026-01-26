#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  debug( "receive() called" );
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( !rec_syn_ ) {
    if ( message.SYN ) {
      rec_syn_ = true;
      zero_point_ = message.seqno;
    } else {
      return;
    }
  }
  checkpoint_ = reassembler_.writer().bytes_pushed();
  uint64_t first_index = message.seqno.unwrap( zero_point_, checkpoint_ ) - ( message.SYN ? 0 : rec_syn_ );
  reassembler_.insert( first_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  debug( "send() called" );
  TCPReceiverMessage tcprm;
  if ( rec_syn_ ) {
    uint64_t absolute_ackno = reassembler_.writer().bytes_pushed() + 1;
    if ( writer().is_closed() ) {
      absolute_ackno += 1;
    }
    tcprm.ackno = Wrap32::wrap( absolute_ackno, zero_point_ );
  } else {
    tcprm.ackno = std::nullopt;
  }
  uint64_t capacity = writer().available_capacity();
  tcprm.window_size = static_cast<uint16_t>( std::min( capacity, static_cast<uint64_t>( UINT16_MAX ) ) );
  tcprm.RST = reassembler_.reader().has_error();
  return tcprm;
}
