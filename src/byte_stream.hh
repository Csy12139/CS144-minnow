#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

class Reader;
class Writer;

class ByteStream
{
private:
  class RingBuffer
  {
  protected:
    std::string m_data {};
    uint64_t m_capacity {};
    uint64_t m_begin {};
    uint64_t m_end {};
    uint64_t m_size {};

  public:
    RingBuffer( uint64_t capacity );
    uint64_t size() const;
    uint64_t bytes_pushed() const;
    void push( const std::string& data, uint64_t count );
    void pop( uint64_t count );
    const char* data() const;
  };

protected:
  uint64_t m_capacity = 0;
  RingBuffer m_buf;
  uint64_t m_bytes_popped = 0;
  uint64_t m_bytes_pushed = 0;

  bool m_closed = false;
  bool m_error = false;
  // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.

public:
  explicit ByteStream( uint64_t capacity );

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;
};

class Writer : public ByteStream
{
public:
  void push( const std::string& data ); // Push data to stream, but only as much as available capacity allows.

  void close();     // Signal that the stream has reached its ending. Nothing more will be written.
  void set_error(); // Signal that the stream suffered an error.

  bool is_closed() const;              // Has the stream been closed?
  uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const; // Peek at the next bytes in the buffer
  void pop( uint64_t len );      // Remove `len` bytes from the buffer

  bool is_finished() const; // Is the stream finished (closed and fully popped)?
  bool has_error() const;   // Has the stream had an error?

  uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t len, std::string& out );
