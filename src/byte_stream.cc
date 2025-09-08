#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : buffer_(), capacity_( capacity ), bytes_pushed_( 0 ), bytes_popped_( 0 ), is_close_( false ), error_( false )
{}

void Writer::push( string data )
{
  uint64_t write_len = min( data.size(), available_capacity() );
  Writer::buffer_ += data.substr( 0, write_len );
  bytes_pushed_ += write_len;
}

void Writer::close()
{
  is_close_ = true;
}

bool Writer::is_closed() const
{
  return is_close_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  std::string_view sv1( buffer_ );
  return { sv1 };
}

void Reader::pop( uint64_t len )
{
  uint64_t pop_len = min( len, bytes_buffered() );
  Reader::buffer_.erase( 0, pop_len );
  bytes_popped_ += pop_len;
}

bool Reader::is_finished() const
{
  return is_close_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}