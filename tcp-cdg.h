#include "ns3/tcp-congestion-ops.h"
// Functions to be implemented by default

namespace ns3{

class TcpCDG : public TcpCongestionOps
{

	virtual std::string GetName () const;

	virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);

	virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

	virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,const Time& rtt);

	virtual Ptr<TcpCongestionOps> Fork ();

	virtual void CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCaEvent_t event);




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

	cdg_state state 	{cdg_state::CGD_UNKNOWN};
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
};

}









