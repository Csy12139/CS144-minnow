#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::RingBuffer::RingBuffer( uint64_t capacity ) : m_capacity( capacity )
{
  m_data.resize( capacity );
}

uint64_t ByteStream::RingBuffer::size() const
{
  return m_size;
}

uint64_t ByteStream::RingBuffer::available_size() const
{
  return m_size == m_capacity || m_end < m_begin ? m_capacity - m_begin : m_end - m_begin;
}

const char* ByteStream::RingBuffer::data() const
{
  return m_data.data() + m_begin;
}

void ByteStream::RingBuffer::append( const std::string& data, uint64_t count )
{
  uint64_t part_left = 0, part_right = 0;

  part_left = m_end + count > m_capacity ? m_capacity - m_end : count;
  part_right = m_end + count > m_capacity ? ( m_end + count ) % m_capacity : 0;

  if ( part_left > 0 )
    m_data.replace( m_end, part_left, data, 0, part_left );

  m_end = ( m_end + count ) % m_capacity;

  if ( part_right > 0 )
    m_data.replace( 0, part_right, data, part_left, part_right );

  m_size += count;
}

void ByteStream::RingBuffer::pop( uint64_t len )
{
  m_size -= len;
  m_begin += len;
  m_begin %= m_capacity;
}

ByteStream::ByteStream( uint64_t capacity ) : m_capacity( capacity ), m_buf( capacity ) {}

void Writer::push( const string& data )
{
  uint64_t len = data.size() <= available_capacity() ? data.size() : available_capacity();
  m_buf.append( data, len );
  m_bytes_pushed += len;
}

void Writer::close()
{
  m_closed = true;
}

void Writer::set_error()
{
  m_error = true;
}

bool Writer::is_closed() const
{
  return m_closed;
}

uint64_t Writer::available_capacity() const
{
  return m_capacity - m_buf.size();
}

uint64_t Writer::bytes_pushed() const
{
  return m_bytes_pushed;
}

string_view Reader::peek() const
{
  return string_view( m_buf.data(), m_buf.available_size() );
}

bool Reader::is_finished() const
{
  return m_closed && bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  return m_error;
}

void Reader::pop( uint64_t len )
{
  len = len > bytes_buffered() ? bytes_buffered() : len;
  m_buf.pop( len );
  m_bytes_popped += len;
}

uint64_t Reader::bytes_buffered() const
{
  return m_buf.size();
}

uint64_t Reader::bytes_popped() const
{
  return m_bytes_popped;
}
