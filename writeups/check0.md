Checkpoint 0 Writeup
====================

My name: 谢升楼

My StudentID: 23302010021

I would like to credit/thank these classmates for their help: I finished this lab by myself.

This lab took me about 3 hours to do. I did attend the lab session.

My secret code from section 2.1 was: 578786

---

**I was surprised by or edified to learn that:**

- 有限容量的字节流只要读者及时读取，就能承载任意长度的逻辑流；容量只限制内存中缓存的字节数，而不限制流的总长度。

- 在 HTTP/1.1 中，每行必须用 CRLF ("\r\n") 结尾，并且明确发送 "Connection: close" 很重要。有了 "close"，服务器在发送完响应后会关闭连接，客户端可以通过 EOF 可靠地判断何时停止读取。

- 一个小的实现细节（比如 shell 脚本的可执行位）就可能阻塞整个 CTest 运行；修复文件权限和修代码同等重要。

---

**Describe ByteStream implementation (short & casual):**

- 我保留的状态：一个连续的 std::string 缓冲区，一个指向下一个未读字节的 head_index，容量上限，累积计数器（bytes_pushed / bytes_popped），以及 closed / error 标志。

- 工作原理（简单）：Writer::push() 把最多等于可用容量的字节追加到缓冲区；Reader::peek() 返回从 head_index 开始的 string_view（零拷贝）；Reader::pop(n) 会把 head_index 向前移动最多 n 个字节并增加已弹出计数。

- 内存处理：不在每次 pop 时都擦除字符串前缀（那样很慢），而是仅移动 head_index。当已消费的前缀变得很大且满足阈值时，会擦除前缀以回收空间——这样实现简单且开销可控。

- 边界情况：close() 之后的 push 会被忽略；push 可能是部分写入（容量不足时）；pop 会限定在当前缓冲字节数内；is_finished() 表示已关闭且缓冲区为空。

- 性能：peek / 可用容量 / 计数器为 O(1)。push/pop 摊销为 O(1)；偶尔的压缩是 O(n) 但很少发生。此机的速度测试约为 18.07 Gbit/s。

- 考虑过的替代方案：环形缓冲（更复杂但可避免压缩）、deque<char>（使 peek 复杂）、或者每次 pop 都擦除前缀（性能差）。

---

**Optional: I had unexpected difficulty with:**

- 构建/测试脚本缺少可执行权限（make-parallel.sh、webget_t.sh），导致运行 `cmake --build ...` 和 `ctest` 时出现 “Permission denied”。解决方法：`chmod +x`。
- 刚刚上手项目，花了些时间熟悉代码框架。

---

**Optional: I think you could make this lab better by:**


---

**Optional: I'm not sure about:**


---

**Optional: I contributed a new test case that catches a plausible bug**

  not otherwise caught: [provide Pull Request URL]
  not otherwise caught: [provide Pull Request URL]

---
