#ifndef SPONGE_LIBSPONGE_TCP_FACTORED_HH
#define SPONGE_LIBSPONGE_TCP_FACTORED_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"

//! \brief A complete endpoint of a TCP connection
class TCPConnection {
  private:
    //一个完全体的TCP端点，包括tcp的配置信息，发送端和接收端
    TCPConfig _cfg;
    TCPReceiver _receiver{_cfg.recv_capacity};
    TCPSender _sender{_cfg.send_capacity, _cfg.rt_timeout, _cfg.fixed_isn};

    //TCP端点想要发送端数据，OS从这里拿数据给网卡。
    //! outbound queue of segments that the TCPConnection wants sent
    std::queue<TCPSegment> _segments_out{};

    //! Should the TCPConnection stay active (and keep ACKing)
    //! for 10 * _cfg.rt_timeout milliseconds after both streams have ended,
    //! in case the remote TCPConnection doesn't know we've received its whole stream?
    //该变量的含义是当tcpconnection看似结束了是否应该逗留？
    //默认自己是主动关闭的一方，当自己是服务器端时才将其置为false（判断当fin报文来临时，自己的入站流结束，但出站流未结束）
    bool _linger_after_streams_finish{true};
    
    //添加一个私有函数，表示将TCPSender的输出队列中的数据，加上一些标识ackno和win，将其放到TCPConnection的输出队列
    void push_sender();

    //自从上次收到receive经过了多少时间
    size_t _time_since_last_received_counter{0};
  public:
    //! \name "Input" interface for the writer
    //!@{对于写应用提供接口

    //建立一个连接，即发送一个SYN报文
    //! \brief Initiate a connection by sending a SYN segment
    void connect();

    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    //应用层的数据过来需要发送
    size_t write(const std::string &data);

    //! \returns the number of `bytes` that can be written right now.
    //返回目前还能往socket写入多少字节
    size_t remaining_outbound_capacity() const;

    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    //关闭outbound 字节流，不再向外输出
    void end_input_stream();
    //!@}

    //! \name "Output" interface for the reader
    //!@{对于接收应用提供接口

    //! \brief The inbound byte stream received from the peer
    //对等点发送的字节
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}

    //! \name Accessors used for testing

    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    //已经发送但仍未被承认的字节数，包括SYN和FIN。
    size_t bytes_in_flight() const;
    //! \brief number of bytes not yet reassembled
    //已经收到但未排序的字节数
    size_t unassembled_bytes() const;
    //! \brief Number of milliseconds since the last segment was received
    //自从上次收到tcp端已经过去的时间？？未实现
    size_t time_since_last_segment_received() const;
    //!< \brief summarize the state of the sender, receiver, and the connection
    //返回当前TCPConnection的发送器，接收器和连接的状态。
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    //从网络中接收一个tcp段
    void segment_received(const TCPSegment &seg);

    //! Called periodically when time elapses
    //时间流逝函数
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    //返回TCPConnection的输出队列，方便所有者调用
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    //当前TCP连接是否应该存活
    bool active() const;
    //!@}

    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH
