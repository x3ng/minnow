#include "byte_stream.hh"
#include "debug.hh"
#include <cstdint>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_( string() ) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( const string& data )
{
  // Your code here (and in each method below)
  const uint64_t remaining = capacity_ - buffer_.size();
  if ( remaining == 0 || data.empty() ) {
    return;
  }
  const uint64_t write_len = std::min( static_cast<uint64_t>( data.size() ), remaining );
  buffer_.append( data.substr( 0, write_len ) );
  bytes_pushed_ += write_len;
  debug( "Writer::push({})", data );
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  debug( "Writer::close()" );
  writer_closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  debug( "Writer::is_closed()" );
  return writer_closed_; // Your code here.
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  debug( "Writer::available_capacity()" );
  const uint64_t available = capacity_ - buffer_.size();
  return available; // Your code here.
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  debug( "Writer::bytes_pushed()" );
  return bytes_pushed_; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  debug( "Reader::peek()" );
  return string_view( buffer_ ); // Your code here.
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  debug( "Reader::pop({})", len );
  if ( len == 0 || buffer_.empty() ) {
    return;
  }
  const uint64_t pop_len = std::min( len, static_cast<uint64_t>( buffer_.size() ) );
  buffer_.erase( 0, pop_len );
  bytes_popped_ += pop_len;
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  debug( "Reader::is_finished()" );
  const bool finished = writer_closed_ && buffer_.empty() && !has_error();
  return finished; // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  debug( "Reader::bytes_buffered()" );
  const uint64_t buffered = buffer_.size();
  return buffered; // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  debug( "Reader::bytes_popped()" );
  return bytes_popped_; // Your code here.
}
