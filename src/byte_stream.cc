#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : m_capacity( capacity ), m_buf() {}

void Writer::push( const string &data )
{
  uint64_t len = data.size() <= available_capacity() ? data.size() : available_capacity();
  m_buf.append(data.begin(), data.begin() + len);
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
  return string_view(m_buf);
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
  m_buf.erase(0, len);
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
