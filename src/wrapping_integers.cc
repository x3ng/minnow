#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // debug( "wrap( {}, {} ) called", n, zero_point.raw_value_ );
  const auto ln = static_cast<uint32_t>( n );
  return Wrap32( zero_point + ln );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // debug( "unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );
  auto ln = static_cast<uint32_t>( checkpoint );
  ln += zero_point.raw_value_;
  const uint32_t sub = this->raw_value_ - ln;
  const uint64_t forward = checkpoint + sub;
  const uint64_t backward = forward - ( 1ULL << 32 );
  if ( backward > forward ) {
    return forward;
  }
  const uint64_t forward_diff = forward - checkpoint;
  const uint64_t backward_diff = checkpoint - backward;
  return forward_diff > backward_diff ? backward : forward;
}
