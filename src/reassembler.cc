#include "reassembler.hh"

#include <algorithm>
#include <utility>
#include <vector>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  const uint64_t data_len = static_cast<uint64_t>( data.size() );
  const uint64_t data_end = first_index + data_len;

  if ( is_last_substring ) {
    eof_index_ = data_end;
  }

  if ( data_len == 0 ) {
    maybe_close();
    return;
  }

  const uint64_t first_unassembled = next_index_;
  const uint64_t first_unacceptable = first_unacceptable_index();

  if ( data_end <= first_unassembled || first_index >= first_unacceptable ) {
    maybe_close();
    return;
  }

  const uint64_t clipped_start = max( first_index, first_unassembled );
  const uint64_t clipped_end = min( data_end, first_unacceptable );
  if ( clipped_start >= clipped_end ) {
    maybe_close();
    return;
  }

  const size_t offset = static_cast<size_t>( clipped_start - first_index );
  const size_t length = static_cast<size_t>( clipped_end - clipped_start );
  string clipped = data.substr( offset, length );

  store_segment( clipped_start, std::move( clipped ) );
  assemble_contiguous();
  maybe_close();
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_bytes_;
}

uint64_t Reassembler::first_unacceptable_index() const
{
  return next_index_ + output_.writer().available_capacity();
}

void Reassembler::store_segment( uint64_t start, std::string&& data )
{
  if ( data.empty() ) {
    return;
  }

  uint64_t merge_start = start;
  uint64_t merge_end = start + static_cast<uint64_t>( data.size() );
  std::vector<std::pair<uint64_t, std::string>> absorbed;

  auto it = segments_.lower_bound( start );
  if ( it != segments_.begin() ) {
    --it;
  }

  while ( it != segments_.end() ) {
    const uint64_t seg_start = it->first;
    const uint64_t seg_end = seg_start + static_cast<uint64_t>( it->second.size() );

    if ( seg_start > merge_end ) {
      break;
    }

    if ( seg_end < merge_start ) {
      ++it;
      continue;
    }

    merge_start = min( merge_start, seg_start );
    merge_end = max( merge_end, seg_end );

    absorbed.emplace_back( seg_start, std::move( it->second ) );
    pending_bytes_ -= seg_end - seg_start;
    it = segments_.erase( it );
  }

  std::string merged( static_cast<size_t>( merge_end - merge_start ), '\0' );
  const auto copy_into = [&]( uint64_t seg_start, const std::string& seg_data ) {
    merged.replace( static_cast<size_t>( seg_start - merge_start ), seg_data.size(), seg_data );
  };

  for ( const auto& [seg_start, seg_data] : absorbed ) {
    copy_into( seg_start, seg_data );
  }

  copy_into( start, data );

  pending_bytes_ += merged.size();
  segments_.emplace( merge_start, std::move( merged ) );
}

void Reassembler::assemble_contiguous()
{
  while ( !segments_.empty() ) {
    auto it = segments_.begin();
    if ( it->first != next_index_ ) {
      break;
    }

    std::string data = std::move( it->second );
    pending_bytes_ -= data.size();
    segments_.erase( it );

    const uint64_t chunk_len = static_cast<uint64_t>( data.size() );
    output_.writer().push( std::move( data ) );
    next_index_ += chunk_len;
  }
}

void Reassembler::maybe_close()
{
  if ( eof_index_.has_value() && *eof_index_ == next_index_ && pending_bytes_ == 0 ) {
    output_.writer().close();
  }
}
