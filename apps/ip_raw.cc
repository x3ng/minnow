#include "socket.hh"

using namespace std;

int main()
{
  // construct an Internet or user datagram here, and send using a RawSocket
  string d;

  d += 0b0100'0101; // 0x45 = 69, ipv4 version
  d += string( 7, 0 );
  d += 64; // ttl time
  d += 5;  // ip protocol 5
  d += string( 6, 0 );
  d += static_cast<unsigned char>( 13 );
  d += static_cast<unsigned char>( 232 );
  d += static_cast<unsigned char>( 197 );
  d += static_cast<unsigned char>( 180 );

  d += "--hello--";
  RawSocket {}.send( d, Address { "192.168.0.1" } );
  return 0;
}
