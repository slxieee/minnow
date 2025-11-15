#include "reassembler.hh"

#include <algorithm>
#include <utility>
#include <vector>

using namespace std;

// 插入一个片段（可能是乱序的）。
// 主要流程：记录EOF(如果是最后一片）->裁剪到当前接收窗口 -> 存 buffer -> 尝试把连续的数据写出去。
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  const uint64_t data_len = static_cast<uint64_t>( data.size() );
  const uint64_t data_end = first_index + data_len;

  // 如果这是最后一片，记下 EOF（绝对索引，one-past-last）。
  if ( is_last_substring ) {
    eof_index_ = data_end;
  }

  // 空片段就没啥好做的，顺便检查是否该关闭流（比如之前已经收到了 EOF）。
  if ( data_len == 0 ) {
    maybe_close();
    return;
  }

  // 剪裁：只接收还没组装的字节，并且不超过接收窗口的上界。
  const uint64_t first_unassembled = next_index_;
  const uint64_t first_unacceptable = first_unacceptable_index();

  // 全部在已组装前面或全部超出窗口的直接丢弃。
  if ( data_end <= first_unassembled || first_index >= first_unacceptable ) {
    maybe_close();
    return;
  }

  const uint64_t clipped_start = max( first_index, first_unassembled );
  const uint64_t clipped_end = min( data_end, first_unacceptable );
  if ( clipped_start >= clipped_end ) {
    // 裁剪后没有剩余字节
    maybe_close();
    return;
  }

  // 取得裁剪后的子串并移动入缓冲处理。
  const size_t offset = static_cast<size_t>( clipped_start - first_index );
  const size_t length = static_cast<size_t>( clipped_end - clipped_start );
  string clipped = data.substr( offset, length );

  store_segment( clipped_start, std::move( clipped ) );
  assemble_contiguous();
  maybe_close();
}

// 返回当前缓冲中的字节总数（尚未写入 output_）。
uint64_t Reassembler::bytes_pending() const
{
  return pending_bytes_;
}

//可接受的第一个不可接受索引（exclusive）：也就是 next_index_ + 剩余写入容量。
uint64_t Reassembler::first_unacceptable_index() const
{
  return next_index_ + output_.writer().available_capacity();
}

//把一个已裁剪好的段存到内部 map，并与相邻/重叠段合并。
// 关键点：我们要移除被吸收的旧段、用一个覆盖整个合并范围的新字符串重建内容，
// 并维护 `pending_bytes_`。所有数据 copy/移动逻辑都在这里。
void Reassembler::store_segment( uint64_t start, std::string&& data )
{
  if ( data.empty() ) {
    return;
  }

  uint64_t merge_start = start;
  uint64_t merge_end = start + static_cast<uint64_t>( data.size() );
  std::vector<std::pair<uint64_t, std::string>> absorbed;

  //先找可能的插入点，向前退一位以便检查和前一个段的重叠情况。
  auto it = segments_.lower_bound( start );
  if ( it != segments_.begin() ) {
    --it;
  }

  // 吸收所有与新段有交集或接触的段。
  while ( it != segments_.end() ) {
    const uint64_t seg_start = it->first;
    const uint64_t seg_end = seg_start + static_cast<uint64_t>( it->second.size() );

    if ( seg_start > merge_end ) {
      break; // 后面的段都在合并区间之后
    }

    if ( seg_end < merge_start ) {
      ++it; // 这个段在合并区间之前，跳过
      continue;
    }

    // 扩展合并区间
    merge_start = min( merge_start, seg_start );
    merge_end = max( merge_end, seg_end );

    absorbed.emplace_back( seg_start, std::move( it->second ) );
    pending_bytes_ -= seg_end - seg_start; // 把被吸收段的长度从 pending 中减去
    it = segments_.erase( it );
  }

  // 用一个空字符串覆盖整个合并区间，然后把吸收进来的数据和新数据放进去。
  std::string merged( static_cast<size_t>( merge_end - merge_start ), '\0' );
  const auto copy_into = [&]( uint64_t seg_start, const std::string& seg_data ) {
    merged.replace( static_cast<size_t>( seg_start - merge_start ), seg_data.size(), seg_data );
  };

  for ( const auto& [seg_start, seg_data] : absorbed ) {
    copy_into( seg_start, seg_data );
  }

  // 把新段的数据拷贝进去（注意 start 是新段在绝对坐标系下的起点）。
  copy_into( start, data );

  pending_bytes_ += merged.size();
  segments_.emplace( merge_start, std::move( merged ) );
}

// 把从 next_index_ 开始能拼成的连续段写进 output_。
// 我们只处理首段等于 next_index_ 的情况，写完后更新 next_index_，
// 继续循环以处理可能的下一个紧接着的段。
void Reassembler::assemble_contiguous()
{
  while ( !segments_.empty() ) {
    auto it = segments_.begin();
    if ( it->first != next_index_ ) {
      break; // 当前最早的段并不是我们期待的索引，不能继续写
    }

    std::string data = std::move( it->second );
    pending_bytes_ -= data.size();
    segments_.erase( it );

    const uint64_t chunk_len = static_cast<uint64_t>( data.size() );
    output_.writer().push( std::move( data ) );
    next_index_ += chunk_len;
  }
}

// 如果已经知道 EOF 且已经把 EOF 之前的字节全部写完了，就关闭 writer。
void Reassembler::maybe_close()
{
  if ( eof_index_.has_value() && *eof_index_ == next_index_ && pending_bytes_ == 0 ) {
    output_.writer().close();
  }
}
