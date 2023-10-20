#include "reassembler.hh"

using namespace std;

Reassembler::Reassembler() : m_buf() {}

void Reassembler::_write_to_stream( Writer& output )
{
  auto iter = m_buf.begin();

  for ( ; iter != m_buf.end() && iter->first == output.bytes_pushed(); ++iter ) {
    output.push( iter->second );
    m_bytes_pending -= iter->second.size();
  }

  m_buf.erase( m_buf.begin(), iter );
}

void Reassembler::_delete_overlapping( uint64_t index )
{
  auto begin = m_buf.find( index );

  if ( begin == m_buf.end() )
    return;

  uint64_t end_index = begin->second.size() + begin->first;
  auto end = ++begin;

  for (; end != m_buf.end() && end->first + end->second.size() <= end_index; ++end )
    m_bytes_pending -= end->second.size();

  m_buf.erase( begin, end );
}

void Reassembler::_insert_to_buffer( uint64_t accept_begin, uint64_t len, uint64_t first_index, string& data )
{
  uint64_t accept_end = accept_begin + len;

  if ( first_index >= accept_end )
    return;

  decltype( m_buf )::iterator iter;

  // Clamp end of data
  uint64_t end_index = first_index + data.size();
  uint64_t clamped_end = accept_end < end_index ? accept_end : end_index;
  iter = m_buf.lower_bound( end_index );

  if ( iter != m_buf.begin() ) {
    --iter;
    uint_fast64_t next_end = iter->first + iter->second.size();
    clamped_end = next_end >= end_index ? iter->first : end_index;
  }

  // Clamp begin of data
  uint64_t clamped_begin = accept_begin > first_index ? accept_begin : first_index;
  iter = m_buf.upper_bound( first_index );

  if ( iter != m_buf.begin() ) {
    --iter;
    uint64_t prev_end = iter->first + iter->second.size();
    clamped_begin = prev_end > first_index ? prev_end : first_index;
  }

  // return if no bytes need to store
  if ( clamped_begin >= clamped_end )
    return;

  // Clamp the data
  data.erase( data.begin() + clamped_end - first_index, data.end() );
  data.erase( data.begin(), data.begin() + clamped_begin - first_index );

  // add to buffer
  m_bytes_pending += data.size();
  m_buf[clamped_begin] = std::move( data );

  _delete_overlapping( clamped_begin );
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // update last index
  if ( is_last_substring )
    m_stream_end = first_index + data.size();

  _insert_to_buffer( output.bytes_pushed(), output.available_capacity(), first_index, data );
  _write_to_stream( output );

  // try to close stream
  if ( output.bytes_pushed() == m_stream_end && !output.is_closed() )
    output.close();
}

uint64_t Reassembler::bytes_pending() const
{
  return m_bytes_pending;
}
