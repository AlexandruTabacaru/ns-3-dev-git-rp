/*
 * Copyright (c) 2014 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

 #ifndef TCPCUBICJUMPSTART_H
 #define TCPCUBICJUMPSTART_H
 
 #include "tcp-congestion-ops.h"
 #include "tcp-socket-base.h"
 
 namespace ns3
 {
 
 /**
  * @brief The Cubic Congestion Control Algorithm with JumpStart instead of HyStart
  */
 class TcpCubicJumpstart : public TcpCongestionOps
 {
   public:
     /**
      * @brief Get the type ID.
      * @return the object TypeId
      */
     static TypeId GetTypeId();
 
     TcpCubicJumpstart();
 
     /**
      * Copy constructor
      * @param sock Socket to copy
      */
     TcpCubicJumpstart(const TcpCubicJumpstart& sock);
 
     std::string GetName() const override;
     void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
     void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
     uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
     void CongestionStateSet(Ptr<TcpSocketState> tcb,
                             const TcpSocketState::TcpCongState_t newState) override;
 
     Ptr<TcpCongestionOps> Fork() override;
     void Init(Ptr<TcpSocketState> tcb) override;
 
   private:
     bool m_fastConvergence; //!< Enable or disable fast convergence algorithm
     bool m_tcpFriendliness; //!< Enable or disable TCP-friendliness heuristic
     double m_beta;          //!< Beta for cubic multiplicative increase
     double m_c;             //!< Cubic Scaling factor
     Time m_cubicDelta;      //!< Time to wait after recovery before update
 
     // JumpStart variables
     bool m_jumpstartDone;       //!< Whether JumpStart has been executed
     uint32_t m_initialBurstSize; //!< Initial burst packet count
     Time m_initialRtt;          //!< Measured or default RTT
 
     // Cubic parameters
     uint32_t m_cWndCnt;        //!<  cWnd integer-to-float counter
     uint32_t m_lastMaxCwnd;    //!<  Last maximum cWnd
     uint32_t m_bicOriginPoint; //!<  Origin point of bic function
     double m_bicK;             //!<  Time to origin point from the beginning of the current epoch
     Time m_delayMin;           //!<  Min delay
     Time m_epochStart;         //!<  Beginning of an epoch
     uint32_t m_ackCnt;         //!<  Count the number of ACKed packets
     uint32_t m_tcpCwnd;        //!<  Estimated tcp cwnd (for Reno-friendliness)
     uint8_t m_cntClamp;     //!< Modulo of the (avoided) float division for cWnd
 
   private:
      /**
      * @brief Perform JumpStart
      * @param tcb Transmission Control Block of the connection
      */
      void JumpStart(Ptr<TcpSocketState> tcb);

     /**
      * @brief Reset Cubic parameters
      * @param tcb Transmission Control Block of the connection
      */
     void CubicReset(Ptr<const TcpSocketState> tcb);
 
     /**
      * @brief Cubic window update after a new ack received
      * @param tcb Transmission Control Block of the connection
      * @param segmentsAcked Segments acked
      * @returns the congestion window update counter
      */
     uint32_t Update(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
 
     /**
      * @brief Send a single segment (used in JumpStart pacing)
      * @param sock The TCP socket base pointer
      */
     void SendOneSegment(Ptr<TcpSocketBase> sock, Ptr<TcpSocketState> tcb);
 };
 
 } // namespace ns3
 
 #endif // TCPCUBICJUMPSTART_H