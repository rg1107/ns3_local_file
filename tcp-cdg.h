#ifndef TCPCDG_H
#define TCPCDG_H
#include "ns3/tcp-congestion-ops.h"
// Functions to be implemented by default

namespace ns3{

class TcpCDG : public TcpCongestionOps
{
	public:

		virtual std::string GetName () const;

		virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);

		virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

		virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,const Time& rtt);

		virtual void CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event);

		virtual Ptr<TcpCongestionOps> Fork ();

		void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

		uint32_t SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

		int tcp_cdg_backoff (Ptr<TcpSocketState> tcb, int32_t grad);

		uint32_t nexp_u32(uint32_t ux);

		int32_t tcp_cdg_grad (Ptr<TcpSocketState> tcb);

		static TypeId GetTypeId (void);

		TcpCDG (void);

		TcpCDG (const TcpCDG& sock);

		virtual ~TcpCDG (void);


	struct minmax {
			union {
				struct {
					int32_t min;
					int32_t max;
					};
					uint64_t v64;
				};
			};
			
	enum cdg_state {
		CDG_UNKNOWN = 0,
		CDG_FULL = 0,
		CDG_NONFULL = 1,
		};


	private:

	cdg_state state 	{cdg_state::CDG_UNKNOWN};
	int window		{0444};
	unsigned int backoff_beta 	{0444};
	unsigned int backoff_factor	{0444};
	unsigned int ineffective_thresh	{0644};
	unsigned int ineffective_hold	{0644};
	struct minmax m_rtt;
	struct minmax m_rtt_prev;
	struct minmax gsum;
	struct minmax *gradients;
	uint32_t rtt_seq;
	uint32_t loss_cwnd;
	unsigned int tail;
	unsigned int backoff_cnt;
	unsigned int delack;
	bool ecn_ce;
	TcpNewReno tnr;
};

} //namespace ns3

#endif // TCPCDG_H









