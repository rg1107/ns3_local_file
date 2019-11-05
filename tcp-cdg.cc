#include "ns3/tcp-cdg.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/log.h"
#include <sys/time.h>
#include <float.h>

namespace ns3
{
	
	NS_LOG_COMPONENT_DEFINE ("TcpCDG");
	NS_OBJECT_ENSURE_REGISTERED (TcpCDG);

	TypeId TcpCDG::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::TcpCDG")
		.SetParent<TcpCongestionOps> () //Doubt
		.AddConstructor<TcpCDG> ()
		.SetGroupName ("Internet")
		.AddAttribute("RTTSequence",
				"Value of rtt sequence", // Edit
				UintegerValue(), // Edit
				MakeUintegerAccessor (&TcpCDG::rtt_seq),
                   		MakeUintegerChecker<uint32_t> ())
		.AddAttribute("LossCwnd",
				"Value of loss congestion window", //Edit
				UintegerValue(), //Edit
				MakeUintegerAccessor (&TcpCDG::loss_cwnd),
                   		MakeUintegerChecker<uint32_t> ())
		.AddAttribute("Tail", 
				"Value of Tail", //Edit
				UintegerValue(), //Edit
				MakeUintegerAccessor (&TcpCDG::tail),
                   		MakeUintegerChecker<unsigned int> ())
		.AddAttribute("BackoffCount",
				"Value of backoff count", //Edit
				UintegerValue(), //Edit
				MakeUintegerAccessor (&TcpCDG::backoff_cnt),
                   		MakeUintegerChecker<unsigned int> ())
		.AddAttribute("Delack", 
				"Value of Delack", //Edit
				UintegerValue(), //Edit
				MakeUintegerAccessor (&TcpCDG::backoff_cnt),
                   		MakeUintegerChecker<unsigned int> ())
		.AddAttribute("ECN_ce",
				"Value of ECN_ce", 
				BoolValue(), //Edit
				MakeBoolAccessor (&TcpCDG::ecn_ce), //Edit
				MakeBoolChecker<bool> ())
		;
				
		
		return tid;
		
	}
	
	//Default Constructor
	//Assign default values to attributes here
	TcpCDG::TcpCDG (void)
	:TcpCongestionOps (), //Doubt

	{
		NS_LOG_FUNCTION (this);
  		NS_LOG_INFO("CDG");
	}
	


	
	//Parametrized Constructor taking CDG socket
	//Assign default values to attributes here

	TcpCDG::TcpCDG (const TcpCDG &sock)
	:TcpCongestionOps (sock), // Doubt
	window(sock.window)
	backoff_beta(sock.backoff_beta)
	backoff_factor(sock.backoff_factor)
	ineffective_thresh(sock.ineffective_thresh)
	ineffective_hold(sock.ineffective_hold)
	rtt_seq (sock.rtt_seq),
	loss_cwnd (sock.loss_cwnd),
	tail (sock.tail),
	backoff_cnt (sock.backoff_cnt),
	ecn_ce (sock.ecn_ce)
	
	{
		NS_LOG_FUNCTION (this);
	}
	
	TcpCDG::~TcpCDG (void)
	{
		NS_LOG_FUNCTION (this);
	}

	static uint32_t TcpCDG::nexp_u32(uint32_t ux)
	{
		static const uint16_t v[] = {
			/* exp(-x)*65536-1 for x = 0, 0.000256, 0.000512, ... */
					65535,65518, 65501, 65468, 65401, 65267, 65001, 64470, 63422,
					61378, 57484, 50423, 38795, 22965, 8047, 987,14,
					};
		uint64_t res;
		uint32_t msb = ux >> 8;
		int i;
		/* Cut off when ux >= 2^24 (actual result is <= 222/U32_MAX). */
		if (msb > USHRT_MAX)
			return 0;
		/* Scale first eight bits linearly: */
		res = UINT_MAX - (ux & 0xff) * (UINT_MAX / 1000000);

		/* Obtain e^(x + y + ...) by computing e^x * e^y * ...: */
		for (i = 1; msb; i++, msb >>= 1) {
			uint64_t y = v[i & -(msb & 1)] + 1ULL;
			res = (res * y) >> 16;
		}

		return (uint32_t)res;
	}

	static int32_t TcpCDG::tcp_cdg_grad (Ptr<TcpSocketState> tcb)
	{
		//Write the code here
		int32_t grad = 0;
		if(m_rtt_prev.v64)
		{
			int32_t gmin = m_rtt.min - m_rtt_prev.min;
			int32_t gmax = m_rtt.max - m_rtt_prev.max;
			int32_t gmin_s;
			int32_t gmax_s;
			
			gsum.min += gmin - gradients[tail].min;
			gsum.max += gmax - gradients[tail].max;
			gradients[tail].min = gmin;
			gradients[tail].max = gmax;
			tail = (tail + 1) & (window - 1); //update window parameter in add attribute(probably)

			/* We keep sums to ignore gradients during CWR;
			* smoothed gradients otherwise simplify to:
			* (rtt_latest - rtt_oldest) / window.
			*/
			gmin_s = DIV_ROUND_CLOSEST(gsum.min, window);
			gmax_s = DIV_ROUND_CLOSEST(gsum.max, window);

			/* Only use smoothed gradients in CA: */
			if (tcb.m_cWnd > tcb.m_ssThresh) {
				grad = gmin_s > 0 ? gmin_s : gmax_s;
			} else {
				/* Prefer unsmoothed gradients in slow start. */
				grad = gmin > 0 ? gmin : gmin_s;
				if (grad < 0)
					grad = gmax > 0 ? gmax : gmax_s;
				}
			if (gmin_s > 0 && gmax_s <= 0)
				state = CDG_FULL;
			else if ((gmin_s > 0 && gmax_s > 0) || gmax_s < 0)
				state = CDG_NONFULL;

			/* Empty queue: */
			//if (gmin_s >= 0 && gmax_s < 0)
				//ca->shadow_wnd = 0;

			/* Backoff was effectual: */
			if (gmin_s < 0 || gmax_s < 0)
				backoff_cnt = 0;	

		}

		m_rtt_prev = m_rtt;
		m_rtt.v64 = 0;
		return grad;

	}

	static int TcpCDG::tcp_cdg_backoff (Ptr<TcpSocketState> tcb, int32_t grad)
	{
		//Write the code here
		//substitute random number generator for prandom_u32() and add backoff factor, ineffective_thresh, ineff_hold as attributes
		if (grad <= 0 || prandom_u32() <= nexp_u32(grad * backoff_factor)) 
			return 0;
		
		backoff_cnt++;

		if (backoff_cnt > ineffective_thresh && ineffective_thresh) {
			if (backoff_cnt >= (ineffective_thresh + ineffective_hold))
				backoff_cnt = 0;
			return 0;
		}

	//Find out tcb's corresponding attributes for below attributes. If not present initialise them in .h and .cc files

		/* reset TLP and prohibit cwnd undo: */
		//tp->tlp_high_seq = 0;
		//tp->prior_ssthresh = 0;
		tcb.m_initialSsThresh = 0;
		/* set PRR target and enter CWR: */
		tcb.m_ssThresh = std::max(2U, (tcb.m_cWnd * backoff_beta) >> 10U);
		//tp->prr_delivered = 0;
		//tp->prr_out = 0;
		tcb.m_initialCWnd = tcb.m_cWnd;
		tcb.m_highTxMark = tcb.m_nextTxSequence;

		tcb.m_congState = TcpSocketState::CA_CWR;
		return 1;
			
	}

	void TcpCDG::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
	{
		//Write the code here
		if (unlikely(!gradients)) // Get the meaning of this function
			return tcp_reno_cong_avoid(sk, ack, acked); // Find the corresponding function
		/* Measure filtered gradients once per RTT: */

		if (after(segmentsAcked, rtt_seq) && rtt.v64) {
			int32_t grad = tcp_cdg_grad(tcb);
			rtt_seq = tcb.m_nextTxSequence;

			if (tcp_cdg_backoff(tcb, grad))
				return;

		} else if (tcb.m_cWnd > tcb.m_ssThresh) {
			/* In CA, synchronize cwnd growth with delay gradients. */
			return;
		}

		//if (!tcp_is_cwnd_limited(sk)) { 
		//	ca->shadow_wnd = min(ca->shadow_wnd, tp->snd_cwnd);
		//	return;
		//}

		if (tcb.m_cWnd <= tcb.m_ssThresh)
			tcp_slow_start(tp, acked); //Find Equivalent function

		else if (tcb.m_cWnd < tp->snd_cwnd_clamp) //Find equivalent variable of snd_cwnd_clamp
			tcb.m_cWnd++;

		//if (ca->shadow_wnd && ca->shadow_wnd < tp->snd_cwnd_clamp)
			//ca->shadow_wnd = max(tp->snd_cwnd, ca->shadow_wnd + 1);

		
	}

	void TcpCDG::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,const Time& rtt)
	{
		//Write the code here

		if(rtt.GetMilliSeconds() <=0)
		{
			return;
		}
		if (segmentsAcked == 1 && delack)
		{
			m_rtt.min = std::min (m_rtt.min, rtt.GetMilliSeconds());
			delack--;
			return;
		}else if(segmentsAcked > 1 && delack < 5) {
			delack++;
		}

		m_rtt.min = min_not_zero(m_rtt.min, rtt.GetMilliSeconds()); //Update this line
		m_rtt.max = std::max(m_rtt.max, rtt.getMilliSeconds());
	}

	uint32_t TcpCDG::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
	{
		//Write the code here
		if (unlikely(!gradients)) //Get the meaning of the function
			return tcp_reno_ssthresh(sk);//Do the same as above

		loss_cwnd = tcb.m_cWnd;
		if (state == CDG_NONFULL && ecn_ce)
			return tcb.m_cWnd;
		//shadow_wnd >>= 1;
		//if (!use_shadow)
			//return max(2U, tp->snd_cwnd >> 1);
		return std::max(2U,tcb.m_cWnd >> 1);
		
	}

	void TcpCDG::CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCaEvent_t event)
	{
		//Write the code here
		switch (event) {
			case CA_EVENT_ECN_NO_CE:
				ecn_ce = false;
				break;
			case CA_EVENT_ECN_IS_CE:
				ecn_ce = true;
				state = CDG_UNKNOWN;
				break;
			case CA_EVENT_CWND_RESTART:
				if (gradients)
					memset(gradients, 0, window * sizeof(gradients[0]));
				memset(ca, 0, sizeof(*ca)); //Change this
				rtt_seq = tcb.m_nextTxSequence; //Look for correct datatypes
				//shadow_wnd = tcb.m_cWnd;
				break;
			case CA_EVENT_COMPLETE_CWR:
				state = CDG_UNKNOWN;
				rtt_seq = tcb.m_nextTxSequence;
				m_rtt_prev = m_rtt;
				m_rtt.v64 = 0;
				break;
			default:
				break;
		}
	}
	

	std::string TcpCDG::GetName () const
	{
  		return "CDG";
	}

	Ptr<TcpCongestionOps> TcpCDG::Fork (void)
	{
  		return CopyObject<TcpCDG> (this);
	}
		
		
}		
