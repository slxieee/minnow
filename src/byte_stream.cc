#include "byte_stream.hh"

#include "byte_stream.hh"

#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( data.empty() || closed_ ) {
    return;
  }

  const uint64_t writable = available_capacity();
  if ( writable == 0 ) {
    return;
  }

  const uint64_t to_write = min<uint64_t>( writable, data.size() );
  buffer_.append( data.data(), static_cast<size_t>( to_write ) );
  bytes_pushed_ += to_write;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - static_cast<uint64_t>( buffer_.size() - head_index_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  const size_t buffered = static_cast<size_t>( bytes_buffered() );
  if ( buffered == 0 ) {
    return {};
  }

  return string_view { buffer_.data() + head_index_, buffered };
}

void Reader::pop( uint64_t len )
{
  const uint64_t buffered = bytes_buffered();
  const uint64_t to_pop = min( len, buffered );
  if ( to_pop == 0 ) {
    return;
  }

  head_index_ += static_cast<size_t>( to_pop );
  bytes_popped_ += to_pop;

  if ( head_index_ == buffer_.size() ) {
    buffer_.clear();
    head_index_ = 0;
  } else if ( head_index_ > 4096 && head_index_ >= buffer_.size() / 2 ) {
    buffer_.erase( 0, head_index_ );
    head_index_ = 0;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return static_cast<uint64_t>( buffer_.size() - head_index_ );
}
