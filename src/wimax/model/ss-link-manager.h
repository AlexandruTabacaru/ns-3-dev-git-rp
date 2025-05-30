/*
 * Copyright (c) 2007,2008,2009 INRIA, UDcast
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Jahanzeb Farooq <jahanzeb.farooq@sophia.inria.fr>
 *          Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 *                               <amine.ismail@UDcast.com>
 */

#ifndef LINK_MANAGER_H
#define LINK_MANAGER_H

#include "cid.h"
#include "mac-messages.h"
#include "ss-net-device.h"
#include "wimax-net-device.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"

#include <stdint.h>

namespace ns3
{

/**
 * @ingroup wimax
 * @brief this class implements the link manager of subscriber station net device. An ss link
 * manager is responsible for link scanning and ranging
 */
class SSLinkManager : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * Constructor
     *
     * @param ss subscriber station device
     */
    SSLinkManager(Ptr<SubscriberStationNetDevice> ss);
    ~SSLinkManager() override;
    void DoDispose() override;

    /**
     * Set BS EIRP
     * @param bs_eirp the BS EIRP
     */
    void SetBsEirp(uint16_t bs_eirp);
    /**
     * Set EIRX IR maximum
     * @param eir_x_p_ir_max the EIRX IR maximum
     */
    void SetEirXPIrMax(uint16_t eir_x_p_ir_max);
    /**
     * Set ranging interval found
     * @param rangingIntervalFound the ranging interval found
     */
    void SetRangingIntervalFound(bool rangingIntervalFound);
    /**
     * Get ranging interval found
     * @returns the ranging interval found
     */
    bool GetRangingIntervalFound() const;
    /**
     * Set NR ranging trans opps
     * @param nrRangingTransOpps the NR ranging trans opps
     */
    void SetNrRangingTransOpps(uint8_t nrRangingTransOpps);
    /**
     * Set ranging CW
     * @param rangingCW the ranging CW
     */
    void SetRangingCW(uint8_t rangingCW);
    /// Increment NR invited polls received
    void IncrementNrInvitedPollsRecvd();
    /**
     * Get DL map sync timeout event
     * @returns the event ID
     */
    EventId GetDlMapSyncTimeoutEvent();

    /**
     * Perform ranging
     * @param cid the CID
     * @param rngrsp the ranging response
     */
    void PerformRanging(Cid cid, RngRsp rngrsp);
    /**
     * Start scanning
     * @param type the event type
     * @param deleteParameters the delete parameters
     */
    void StartScanning(SubscriberStationNetDevice::EventType type, bool deleteParameters);
    /**
     * Send ranging request
     * @param uiuc the UIUC
     * @param allocationSize the allocation size
     */
    void SendRangingRequest(uint8_t uiuc, uint16_t allocationSize);
    /// Start contention resolution
    void StartContentionResolution();
    /// Perform backoff
    void PerformBackoff();
    /**
     * Is UL channel usable
     * @returns the UL channel usable flag
     */
    bool IsUlChannelUsable();
    /**
     * Schedule scanning request
     * @param interval the scanning request interval
     * @param eventType event type
     * @param deleteUlParameters the delete UL parameters
     * @param eventId the event ID
     */
    void ScheduleScanningRestart(Time interval,
                                 SubscriberStationNetDevice::EventType eventType,
                                 bool deleteUlParameters,
                                 EventId& eventId);

  private:
    /// type conversion operator
    SSLinkManager(const SSLinkManager&);
    /**
     * assignment operator
     * @returns SS link manager
     */
    SSLinkManager& operator=(const SSLinkManager&);

    /**
     * End scanning
     * @param status the end status
     * @param frequency the frequency
     */
    void EndScanning(bool status, uint64_t frequency);
    /// Start synchronizing
    void StartSynchronizing();
    /**
     * Search for DL channel
     * @param channel the DL channel
     * @returns true if found
     */
    bool SearchForDlChannel(uint8_t channel);
    /// Select random backoff
    void SelectRandomBackoff();
    /// Increase rnaging request CW
    void IncreaseRangingRequestCW();
    /// Reset ranging request CW
    void ResetRangingRequestCW();
    /// Delete uplink parameters
    void DeleteUplinkParameters();
    /**
     * Adjust ranging parameters
     * @param rngrsp the ranging response
     */
    void AdjustRangingParameters(const RngRsp& rngrsp);
    /// Negotiate basic capabilities
    void NegotiateBasicCapabilities();
    /**
     * Calculate maximum IR signal strength
     * @returns the maximum IR signal strength
     */
    uint16_t CalculateMaxIRSignalStrength();
    /**
     * Get minimum transmit power level
     * @returns the minimum transmit power level
     */
    uint16_t GetMinTransmitPowerLevel();

    Ptr<SubscriberStationNetDevice> m_ss; ///< subscriber station device

    WimaxNetDevice::RangingStatus m_rangingStatus; ///< ranging status
    // initial ranging parameters obtained from DCD (in channel encodings)
    uint16_t m_bsEirp;     ///< BS EIRP
    uint16_t m_eirXPIrMax; ///< initial ranging maximum equivalent isotropic received power at BS
    uint16_t m_pTxIrMax; ///< maximum transmit signal strength for initial ranging calculated by SS

    uint8_t m_initRangOppNumber; ///< Initial Ranging opportunity (1–255) in which SS transmitted
                                 ///< the RNG_REQ
    uint8_t m_contentionRangingRetries; ///< contention ranging retries
    uint32_t m_rngReqFrameNumber;       ///< frame number in which SS sent RNG_REQ message
    RngReq m_rngreq;                    ///< rng request

    uint8_t m_dlChnlNr;   ///< indicates the channel/frequency currently the SS is scanning
    uint64_t m_frequency; ///< frequency on which it is currently operating, i.e., where scanning
                          ///< was successful
    bool m_rangingIntervalFound; ///< ranging interval found

    // stats members
    uint16_t m_nrRngReqsSent;       ///< number rang requests sent
    uint16_t m_nrRngRspsRecvd;      ///< number rang responses received
    uint16_t m_nrInvitedPollsRecvd; ///< number invited polls received

    uint8_t m_rangingCW;          ///< ranging CW
    uint8_t m_rangingBO;          ///< ranging BO
    uint8_t m_nrRangingTransOpps; ///< number ranging trans opps
    bool m_isBackoffSet;          ///< is backoff set
    uint8_t m_rangingAnomalies;   ///< ranging anomalies

    EventId m_waitForRngRspEvent;    ///< wait for rang response event
    EventId m_dlMapSyncTimeoutEvent; ///< DL map sync timeout event
};

} // namespace ns3

#endif /* LINK_MANAGER_H */
