# Checkpoint 1 Writeup

My name: 谢升楼

My StudentID: 23302010021

I would like to credit/thank these classmates for their help: [None]

This lab took me about 3.5 hours to do. I did attend the lab session.

---

I was surprised by or edified to learn that: 
简单的重组器要对容量、窗口边界和重复/重叠数据处理得这么严格才能通过所有测试。特别是first unassembled / first unacceptable index的窗口模型，让我更直观地理解了 TCP 接收端是如何在有限内存下进行有界缓冲的。

---

Describe Reassembler structure and design. [Describe data structures and
approach taken. Describe alternative designs considered or tested.
Describe benefits and weaknesses of your design compared with
alternatives -- perhaps in terms of simplicity/complexity, risk of
bugs, asymptotic performance, empirical performance, required
implementation time and difficulty, and other factors. Include any
measurements if applicable.]

对于 Reassembler，我只保存三类信息：

- 要写入输出的下一个字节索引（`next_index_`）。
- 尚未组装的片段集合（`segments_`），以起始索引为键，使用 `std::map<uint64_t, std::string>` 存储。
- 记录信息：已缓冲字节总数（`pending_bytes_`）和可选的流结尾索引（`eof_index_`）。

**数据结构与方法**

- `std::map<uint64_t, std::string> segments_`  
  每个键是一个待组装片段的起始索引，值是一个连续且不重叠的子串。保证：
  - map 中的所有片段互不重叠；
  - 每个片段尽可能合并，不存在可合并的相邻片段

- `next_index_`  
  跟踪第一个未组装的索引（即下一个应写入 ByteStream 的字节）。当存在以 `next_index_` 为起点的片段时，持续写入输出直到出现缺口。

- `pending_bytes_`  
  恒等于 `segments_` 中所有字符串长度之和，因此 `bytes_pending()` 可在 O(1) 返回该值

- `eof_index_`  
  当 `is_last_substring == true` 时，记录整个流最后一个字节之后的索引（即末端后的索引）。流在满足以下条件时关闭：
  - `eof_index_` 已设置
  - `next_index_ == eof_index_`
  - `pending_bytes_ == 0`
  满足时调用 `output_.writer().close()`

**处理容量 / 窗口**

计算当前滑动窗口：

- `first_unassembled = next_index_`
- `first_unacceptable = next_index_ + output_.writer().available_capacity()`

当新片段 `(first_index, data)` 到达时，先计算其区间 `[first_index, first_index + len)`，并将其裁剪到当前“可接受窗口”与“未组装部分”的交集：

- 若片段在或早于 `first_unassembled` 结束，则完全为已组装部分 → 丢弃。
- 若片段在或晚于 `first_unacceptable` 开始，则超出容量 → 丢弃。
- 否则裁剪为：
  - `clipped_start = max(first_index, first_unassembled)`
  - `clipped_end = min(data_end, first_unacceptable)`  
  仅保留 `[clipped_start, clipped_end)`

这样保证：
- 不会存储超出 ByteStream 当前可用容量的字节
- 不会存储已被组装或已弹出的字节

**合并重叠片段**

插入裁剪后的片段时调用 `store_segment(start, std::string&& data)`：

1. 用 `segments_.lower_bound(start)`（可能向前退一位）查找所有与 `[start, start+len)` 重叠或相邻的已有片段
2. 对每个重叠片段：
   - 更新 `merge_start = min(merge_start, seg_start)` 和 `merge_end = max(merge_end, seg_end)`
   - 从 `segments_` 中移除旧片段，并从 `pending_bytes_` 中减去其长度
3. 分配一个覆盖 `[merge_start, merge_end)` 的新字符串 `merged`
4. 将所有被吸收的片段和新数据拷贝到 `merged` 的正确偏移处
5. 将 `merged` 以单个片段插回 `segments_`，并将其长度加到 `pending_bytes_`

这样确保任意连续区间最多只有一个片段，消除冗余重叠并保持 map 紧凑。

**组装写入 ByteStream**

存储片段后调用 `assemble_contiguous()`：

- 只要 `segments_` 非空且最早的片段以恰好 `next_index_` 开始：
  - 将该字符串移出 map；
  - 从 `pending_bytes_` 减去其长度；
  - 通过 `output_.writer().push(...)` 推入输出；
  - 将 `next_index_` 增加已写入的长度。

然后调用 `maybe_close()` 检查是否已达到 EOF 并需要关闭流。

**备选方案（考虑过的设计）**

- 使用 `std::vector<char>` 或固定大小缓冲按流位置索引：
  - 优点：随机访问 O(1)；易于标记哪些字节已存在。
  - 缺点：对稀疏索引会占用大量内存；需额外跟踪窗口内哪些位置已填充；移动窗口和容量约束实现更复杂。

- 存储原始区间 `[start,end)` 并配合单独缓冲：
  - 优点：在存在大空洞时更节省空间。
  - 缺点：需更多书面化管理：把区间与实际字节缓冲映射，合并区间和合并缓冲更复杂。

选择 `std::map<uint64_t, std::string>` 的原因：
- 直观、易于与测试语义对应；
- 合并与裁剪逻辑容易推理，降低出错风险；
- 渐近性能可接受：每次插入/合并为 O(k log n)（n 为片段数，k 为被吸收的片段数，通常很小）；
- 实验测试（包括速度测）通过，性能足够。

---

Implementation Challenges:

[1] Correctly handling the capacity window.  
一开始我只根据 `first_unassembled` 做剪裁，忘了考虑 `first_unacceptable`，导致有些测试中，reassembler 会存下超过 capacity 的数据。修正思路是把“当前位置 + available_capacity()”作为 `first_unacceptable`，对每个新 segment 做区间裁剪和丢弃。

[2] 处理重叠和重复片段。  
重叠场景尤其多（`reassembler_overlapping.cc` 里的测试），一开始我简单地插入 `std::map` 并在遇到重叠时只做部分覆盖，结果 `bytes_pending()` 和 `bytes_pushed()` 都和预期不一致。最后采用了“吸收所有重叠片段、统一合并后重新插入”的方案，保证 map 中每个区间不重叠。

[3] EOF / is_finished 判定。  
`is_last_substring` 可能在有“洞”的时候提前到来（比如先到达流结尾的一部分，再补前面的内容）。一开始我只在收到 last substring 时立即 close，导致有的测试期望 `is_finished == false` 时已经关闭。最后把逻辑改成：记录 `eof_index_`，只有当 `next_index_ == eof_index_` 且 `bytes_pending() == 0` 时才关闭 `ByteStream`。


---

Remaining Bugs:
according to the test bench performance, no known bug.

---

- Optional: I had unexpected difficulty with: [describe]

---

- Optional: I think you could make this lab better by: [describe]

---

- Optional: I'm not sure about: [describe]
