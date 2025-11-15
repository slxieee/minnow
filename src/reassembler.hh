#pragma once

#include "byte_stream.hh"

#include <map>
#include <optional>
#include <string>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_; // the Reassembler writes to this ByteStream

  uint64_t next_index_ {};
  std::map<uint64_t, std::string> segments_ {};
  uint64_t pending_bytes_ {};
  std::optional<uint64_t> eof_index_ {};

  [[nodiscard]] uint64_t first_unacceptable_index() const;
  void store_segment( uint64_t start, std::string&& data );
  void assemble_contiguous();
  void maybe_close();
};
