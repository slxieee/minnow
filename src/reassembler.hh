#pragma once

#include "byte_stream.hh"

#include <map>
#include <optional>
#include <string>

/*
 * 这是个比较朴素的重组器：接收乱序的字节片段（每片段带有绝对起始索引），
 * 把它们拼成按序的连续字节，然后塞到一个`ByteStream`里。我们会记录一个
 * 可选的 EOF 索引：当写到 EOF 并且缓存里没东西时会把输出流关掉。
 */
class Reassembler
{
public:
  // 用给定的`ByteStream构造 Reassembler。
  // 这里接受一个右值并用 move 搬走：重组器在其生命周期内拥有这个流。
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  // 插入一个从绝对索引 `first_index` 开始的字节片段。
  // `data` 是片段内容，如果 `is_last_substring` 为真，表示这是最后一片，
  // 同时记录 EOF（即 first_index + data.size()）。
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // 当前还在重组器内部缓存的字节数（还没写到输出流）。
  // 这个值是所有段的字节总和，常用于测试或流量控制逻辑。
  uint64_t bytes_pending() const;

  // 暴露底层输出流的 Reader，外部可以通过它读取已经组装好的数据。
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // 对外只提供对 Writer 的只读访问（不能从外部写入）。重组器自己负责
  // 把拼好的字节 push 到这个 writer。
  const Writer& writer() const { return output_.writer(); }

private:
  // 我们把组装好的字节写到这个 ByteStream 里。
  ByteStream output_;

  // 下一个我们期望写入输出的绝对索引（即“第一个未组装的字节”的索引）。
  uint64_t next_index_ {};

  // 存放还没写进去的段，键是段起始索引，值是该段的连续字节数据。
  // 注意：我们合并重叠/相邻的段，保证 `segments_`中不会出现重叠区域。
  std::map<uint64_t, std::string> segments_ {};

  // 当前保存在 `segments_` 中的字节总数。
  uint64_t pending_bytes_ {};

  // 如果有值，表示 EOF 的绝对索引（等于最后一个字节后一位）。
  // 当我们把字节写到这个索引时可以安全地关闭 writer。
  std::optional<uint64_t> eof_index_ {};

  // 辅助：返回第一个不可接受的索引（接收窗口的上界，exclusive）。
  // 在 .cc 文件中实现。
  [[nodiscard]] uint64_t first_unacceptable_index() const;

  // 将一个已经裁剪好的段存入缓冲区，并与邻居合并（如果需要）。
  // `data`会被移动进来；函数会维护 `segments_` 和`pending_bytes_`。
  void store_segment( uint64_t start, std::string&& data );

  // 尝试把从`next_index_` 开始的连续数据吐到输出流，直到不再连续为止。
  void assemble_contiguous();

  // 如果知道 EOF 且已经把所有数据写到 EOF，就把 writer 关掉。
  // 这个检查会在状态改变后被多处调用。
  void maybe_close();
};
