/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 NITK Surathkal
 * Copyright (c) 2019 Tom Henderson (update to IETF draft -10)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Shravya K.S. <shravya.ks0@gmail.com>
 */

#ifndef DUAL_PI2_QUEUE_DISC_H
#define DUAL_PI2_QUEUE_DISC_H

#include "queue-disc.h"

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/timer.h"
#include "ns3/traced-value.h"

#include <bitset>
#include <queue>
#include <set>
#include <utility>

namespace ns3
{

class UniformRandomVariable;

/**
 * \ingroup traffic-control
 *
 * Implements Dual Queue Coupled AQM (RFC 9332) queue disc.  The class name
 * derives from the Linux 'sch_dualpi2.c' implementation.
 * The following differences exist with respect to what is specified in
 * RFC 9332:  TBD
 */
class DualPi2QueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * \brief Constructor
     */
    DualPi2QueueDisc();
    /**
     * \brief  Destructor
     */
    ~DualPi2QueueDisc() override;
    /**
     * \brief Get the current value of the queue in bytes.
     *
     * \returns The queue size in bytes.
     */
    uint32_t GetQueueSize() const;
    /**
     * \brief Set the limit of the queue in bytes.
     *
     * \param lim The limit in bytes.
     */
    void SetQueueLimit(uint32_t lim);

    // Reasons for dropping packets
    static constexpr const char* UNFORCED_CLASSIC_DROP =
        "Unforced drop in classic queue"; //!< Early probability drops: proactive
    static constexpr const char* FORCED_DROP =
        "Forced drop"; //!< Drops due to queue limit: reactive
    static constexpr const char* UNFORCED_CLASSIC_MARK =
        "Unforced classic mark"; //!< Unforced mark in classic queue
    static constexpr const char* UNFORCED_L4S_MARK = "Unforced mark in L4S queue";

    /**
     * Callback to be notified of pending bytes about to be dequeued.
     *
     * \param oldBytes Old value of the quantity (ignore)
     * \param newBytes New value of the quantity (use this value)
     */
    void PendingDequeueCallback(uint32_t oldBytes, uint32_t newBytes);

  protected:
    // Documented in base class
    void DoDispose() override;

  private:
    // Documented in base class
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;

    /**
     * \brief Add a QueueDiscItem to the internal L4S staging queue
     * \param qdItem The QueueDiscItem to add
     */
    void AddToL4sStagingQueue(Ptr<QueueDiscItem> qdItem);
    /**
     * \brief Add a QueueDiscItem to the internal CLASSIC staging queue
     * \param qdItem The QueueDiscItem to add
     */
    void AddToClassicStagingQueue(Ptr<QueueDiscItem> qdItem);
    /**
     * \brief Dequeue from the internal L4S staging queue
     * \return The next QueueDiscItem if available, or nullptr
     */
    Ptr<QueueDiscItem> DequeueFromL4sStagingQueue();
    /**
     * \brief Dequeue from the internal CLASSIC staging queue
     * \return The next QueueDiscItem if available, or nullptr
     */
    Ptr<QueueDiscItem> DequeueFromClassicStagingQueue();

    /**
     * \brief Return the Laqm probability of marking
     * \param The QueueDiscItem to evaluate
     * \return The probability of mark, p'L
     */
    double GetNativeLaqmProbability(Ptr<const QueueDiscItem> qdItem) const;

    Ptr<QueueDiscItem> DequeueFromL4sQueue(bool& marked);

    Ptr<QueueDiscItem> DequeueFromClassicQueue(bool& dropped);
    /**
     * \brief Initialize the queue parameters.
     */
    void InitializeParams() override;
    /**
     * \brief check if traffic is classified as L4S (ECT(1) or CE)
     * \param item the QueueDiscItem to check
     * \return true if ECT(1) or CE, false otherwise
     */
    bool IsL4s(Ptr<QueueDiscItem> item);
    /**
     * \brief Implement the L4S recur function for probabilistic marking/dropping
     * \param [in] count the count variable (updated by the method)
     * \param likelihood the likelihood of marking or dropping
     * \return true if the queue should mark or drop the packet
     */
    bool Recur(double& count, double likelihood);
    /**
     * \brief Periodically calculate the drop probability
     */
    void DualPi2Update();
    /**
     * L4S AQM function
     * \param lqTime Delay to evaluate against threshold
     * \return value between 0 and 1 representing the probability of mark
     */
    double Laqm(Time lqTime) const;
    /**
     * \brief Check whether a subsequent call to Scheduler() will return
     * a packet with size <= byteLimit
     *
     * Checks that there exists at least one packet in the queue, and that
     * the size of the head-of-line packet in either (or both) of the L4S
     * and Classic queues is less than the specified limit.
     *
     * \param byteLimit The byte limit to check against
     * \return true if the above conditions hold, false otherwise
     */
    std::pair<bool, bool> CanSchedule(uint32_t byteLimit) const;
    /**
     * A two-band weighted deficit round robin (WDRR) queue.  If there
     * is no packet in the queue, returns the value NONE.  If at least
     * one packet is in the queue, it will return the value L4S or CLASSIC
     * corresponding to which packet should be dequeued next.  Note that this
     * method does not actually dequeue anything; the individual internal
     * queue that is selected must subsequently be dequeued from.
     *
     * \param eligible whether classic (first) or L4S (second) may be scheduled
     * \return either 0 (Classic), 1 (L4S), or 2 (NONE)
     */
    std::size_t Scheduler(std::pair<bool, bool> eligible);

    // Values supplied by user
    Time m_target;              //!< Queue delay target for Classic traffic
    Time m_tUpdate;             //!< Time period after which CalculateP () is called
    Time m_tShift;              //!< Scheduler time bias
    uint32_t m_mtu;             //!< Device MTU (bytes)
    double m_alpha;             //!< Parameter to PI Square controller
    double m_beta;              //!< Parameter to PI Square controller
    Time m_minTh;               //!< L4S marking threshold (in time)
    double m_k;                 //!< Coupling factor
    uint32_t m_classicDeficit;  //!< deficit counter for DRR
    uint32_t m_llDeficit;       //!< deficit counter for DRR
    double m_schedulingWeight;  //!< Scheduling weight
    std::bitset<2> m_drrQueues; //!< bitset for weighted DRR
    uint32_t m_drrQuantum;      //!< DRR quantum
    uint32_t m_queueLimit;      //!< Queue limit in bytes / packets
    Time m_startTime;           //!< Start time of the update timer

    // Variables maintained by DualQ Coupled PI2
    Time m_classicQueueTime;   //!< Arrival time of a packet of Classic Traffic
    Time m_lqTime;             //!< Arrival time of a packet of L4S Traffic
    uint32_t m_thLen;          //!< Minimum threshold (in bytes) for marking L4S traffic
    double m_baseProb;         //!< Variable used in calculation of drop probability
    TracedValue<double> m_pCL; //!< Coupled probability
    TracedValue<double> m_pC;  //!< Classic drop/mark probability
    TracedValue<double> m_pL;  //!< L4S mark probability
    TracedCallback<Time> m_traceClassicSojourn; //!< Classic sojourn time
    TracedCallback<Time> m_traceL4sSojourn;     //!< L4S sojourn time
    Time m_prevQ;                               //!< Old value of queue delay
    EventId m_rtrsEvent;      //!< Event used to decide the decision of interval of drop probability
                              //!< calculation
    double m_l4sCount{0};     //! L queue count for likelihood recur
    double m_classicCount{0}; //! C queue count for likelihood recur
    std::list<Ptr<QueueDiscItem>> m_classicStagingQueue; //!< staging queue for CLASSIC
    std::list<Ptr<QueueDiscItem>> m_l4sStagingQueue;     //!< staging queue for L4S
};

} // namespace ns3

#endif
