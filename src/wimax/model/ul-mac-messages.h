/*
 * Copyright (c) 2007,2008,2009 INRIA, UDcast
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Jahanzeb Farooq <jahanzeb.farooq@sophia.inria.fr>
 *          Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 *                               <amine.ismail@UDcast.com>
 */

#ifndef UCD_CHANNEL_ENCODINGS_H
#define UCD_CHANNEL_ENCODINGS_H

#include "ns3/buffer.h"

#include <list>
#include <stdint.h>

namespace ns3
{

/**
 * @ingroup wimax
 * This class implements the UCD channel encodings as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 11.3.1 UCD channel encodings, page 651
 */
class UcdChannelEncodings
{
  public:
    UcdChannelEncodings();
    virtual ~UcdChannelEncodings();

    /**
     * Set BW request opp size
     * @param bwReqOppSize the bandwidth request opp size
     */
    void SetBwReqOppSize(uint16_t bwReqOppSize);
    /**
     * Set range request opp size
     * @param rangReqOppSize the ranging request opp size
     */
    void SetRangReqOppSize(uint16_t rangReqOppSize);
    /**
     * Set frequency
     * @param frequency the frequency
     */
    void SetFrequency(uint32_t frequency);

    /**
     * Get BW request opp size
     * @returns the bandwidth request opp size
     */
    uint16_t GetBwReqOppSize() const;
    /**
     * Get range request opp size
     * @returns the ragning request opp size
     */
    uint16_t GetRangReqOppSize() const;
    /**
     * Get frequency
     * @returns the frequency
     */
    uint32_t GetFrequency() const;

    /**
     * Get size
     * @returns the size
     */
    uint16_t GetSize() const;

    /**
     * Write item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Write(Buffer::Iterator start) const;
    /**
     * Read item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Read(Buffer::Iterator start);

  private:
    /**
     * Write item
     * @param start the iterator
     * @returns the updated iterator
     */
    virtual Buffer::Iterator DoWrite(Buffer::Iterator start) const = 0;
    /**
     * Read item
     * @param start the iterator
     * @returns the updated iterator
     */
    virtual Buffer::Iterator DoRead(Buffer::Iterator start) = 0;

    uint16_t m_bwReqOppSize;   ///< BW request opp size
    uint16_t m_rangReqOppSize; ///< range request opp size
    uint32_t m_frequency;      ///< frequency
};

} // namespace ns3

#endif /* UCD_CHANNEL_ENCODINGS_H */

// ----------------------------------------------------------------------------------------------------------

#ifndef OFDM_UCD_CHANNEL_ENCODINGS_H
#define OFDM_UCD_CHANNEL_ENCODINGS_H

#include <stdint.h>

namespace ns3
{

/**
 * This class implements the OFDM UCD channel encodings as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 11.3.1 UCD channel encodings, page 651
 */
class OfdmUcdChannelEncodings : public UcdChannelEncodings
{
  public:
    OfdmUcdChannelEncodings();
    ~OfdmUcdChannelEncodings() override;

    /**
     * Set SB channel reguest region full parameters
     * @param sbchnlReqRegionFullParams the channel request region full parameters
     */
    void SetSbchnlReqRegionFullParams(uint8_t sbchnlReqRegionFullParams);
    /**
     * Set SB channel for control codes
     * @param sbchnlFocContCodes the channel control codes
     */
    void SetSbchnlFocContCodes(uint8_t sbchnlFocContCodes);

    /**
     * Get SB channel request region full parameters
     * @returns the channel request region full parameters
     */
    uint8_t GetSbchnlReqRegionFullParams() const;
    /**
     * Get SB channel for control codes
     * @returns the channel for control codes
     */
    uint8_t GetSbchnlFocContCodes() const;

    /**
     * Get size
     * @returns the size
     */
    uint16_t GetSize() const;

  private:
    /**
     * Write item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator DoWrite(Buffer::Iterator start) const override;
    /**
     * Read item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator DoRead(Buffer::Iterator start) override;

    uint8_t m_sbchnlReqRegionFullParams; ///< SB channel request region full parameters
    uint8_t m_sbchnlFocContCodes;        ///< SB channel for control codes
};

} // namespace ns3

#endif /* OFDM_UCD_CHANNEL_ENCODINGS_H */

// ----------------------------------------------------------------------------------------------------------

#ifndef OFDM_UL_BURST_PROFILE_H
#define OFDM_UL_BURST_PROFILE_H

#include "ns3/buffer.h"

#include <stdint.h>

namespace ns3
{

/**
 * This class implements the UL burst profile as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 11.3.1.1 Uplink burst profile encodings, page 655
 */
class OfdmUlBurstProfile
{
  public:
    /// UIUC enumeraton
    enum Uiuc
    {
        UIUC_INITIAL_RANGING = 1,
        UIUC_REQ_REGION_FULL,
        UIUC_REQ_REGION_FOCUSED,
        UIUC_FOCUSED_CONTENTION_IE,
        UIUC_BURST_PROFILE_5,
        UIUC_BURST_PROFILE_6,
        UIUC_BURST_PROFILE_7,
        UIUC_BURST_PROFILE_8,
        UIUC_BURST_PROFILE_9,
        UIUC_BURST_PROFILE_10,
        UIUC_BURST_PROFILE_11,
        UIUC_BURST_PROFILE_12,
        UIUC_SUBCH_NETWORK_ENTRY,
        UIUC_END_OF_MAP
    };

    OfdmUlBurstProfile();
    ~OfdmUlBurstProfile();

    /**
     * Set type
     * @param type the type
     */
    void SetType(uint8_t type);
    /**
     * Set length
     * @param length the length
     */
    void SetLength(uint8_t length);
    /**
     * Set UIUC
     * @param uiuc the UIUC
     */
    void SetUiuc(uint8_t uiuc);
    /**
     * Set FEC code type
     * @param fecCodeType the FEC code type
     */
    void SetFecCodeType(uint8_t fecCodeType);

    /**
     * Get type
     * @returns the type
     */
    uint8_t GetType() const;
    /**
     * Get length
     * @returns the length
     */
    uint8_t GetLength() const;
    /**
     * Get UIUC
     * @returns the UIUC
     */
    uint8_t GetUiuc() const;
    /**
     * Get FEC code type
     * @returns the FEC code type
     */
    uint8_t GetFecCodeType() const;

    /**
     * Get size
     * @returns the size
     */
    uint16_t GetSize() const;

    /**
     * Write item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Write(Buffer::Iterator start) const;
    /**
     * Read item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Read(Buffer::Iterator start);

  private:
    uint8_t m_type;   ///< type
    uint8_t m_length; ///< length
    uint8_t m_uiuc;   ///< UIUC

    // TLV Encoded information
    uint8_t m_fecCodeType; ///< FEC code type
};

} // namespace ns3

#endif /* OFDM_UL_BURST_PROFILE_H */

// ----------------------------------------------------------------------------------------------------------

#ifndef UCD_H
#define UCD_H

#include "ns3/header.h"

#include <stdint.h>
#include <vector>

namespace ns3
{

/**
 * This class implements the UCD message as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 6.3.2.3.3 Uplink Channel Descriptor (UCD) message, page 47
 */
class Ucd : public Header
{
  public:
    Ucd();
    ~Ucd() override;

    /**
     * Set configuration change count
     * @param ucdCount the UCD count
     */
    void SetConfigurationChangeCount(uint8_t ucdCount);
    /**
     * Set ranging backoff start
     * @param rangingBackoffStart ranging backoff start
     */
    void SetRangingBackoffStart(uint8_t rangingBackoffStart);
    /**
     * Set ranging backoff end
     * @param rangingBackoffEnd the ranging backoff end
     */
    void SetRangingBackoffEnd(uint8_t rangingBackoffEnd);
    /**
     * Set request backoff start
     * @param requestBackoffStart the request backoff start
     */
    void SetRequestBackoffStart(uint8_t requestBackoffStart);
    /**
     * Set request backoff end
     * @param requestBackoffEnd the request backoff end
     */
    void SetRequestBackoffEnd(uint8_t requestBackoffEnd);
    /**
     * Set channel encodings
     * @param channelEncodings the channel encodings
     */
    void SetChannelEncodings(OfdmUcdChannelEncodings channelEncodings);
    /**
     * Add UL burst profile
     * @param ulBurstProfile the UL burst profile
     */
    void AddUlBurstProfile(OfdmUlBurstProfile ulBurstProfile);
    /**
     * Set NR UL burst profiles
     * @param nrUlBurstProfiles the NR UL burst profiles
     */
    void SetNrUlBurstProfiles(uint8_t nrUlBurstProfiles);

    /**
     * Get configuration change count
     * @returns the configuration change count
     */
    uint8_t GetConfigurationChangeCount() const;
    /**
     * Get ranging backoff start
     * @returns the ranging backoff start
     */
    uint8_t GetRangingBackoffStart() const;
    /**
     * Get ranging backoff end
     * @returns the ranging backoff end value
     */
    uint8_t GetRangingBackoffEnd() const;
    /**
     * Get request backoff start
     * @returns the request backoff start value
     */
    uint8_t GetRequestBackoffStart() const;
    /**
     * Get request backoff end
     * @returns the request backoff end value
     */
    uint8_t GetRequestBackoffEnd() const;
    /**
     * Get channel encodings
     * @returns the OFDM UCD channel encodings
     */
    OfdmUcdChannelEncodings GetChannelEncodings() const;
    /**
     * Get UL burst profiles
     * @returns the UL burst profile
     */
    std::vector<OfdmUlBurstProfile> GetUlBurstProfiles() const;
    /**
     * Get number UL burst profiles
     * @returns the number of UL burst profiles
     */
    uint8_t GetNrUlBurstProfiles() const;

    /**
     * Get name
     * @returns the name string
     */
    std::string GetName() const;
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint8_t m_configurationChangeCount;         ///< configuration change count
    uint8_t m_rangingBackoffStart;              ///< ranging backoff start
    uint8_t m_rangingBackoffEnd;                ///< ranging backoff end
    uint8_t m_requestBackoffStart;              ///< request backoff start
    uint8_t m_requestBackoffEnd;                ///< request backoff end
    OfdmUcdChannelEncodings m_channelEncodings; ///< TLV Encoded information for the overall channel
    std::vector<OfdmUlBurstProfile> m_ulBurstProfiles; ///< UL burst profiles

    uint8_t m_nrUlBurstProfiles; ///< number UL burst profiles
};

} // namespace ns3

#endif /* UCD_H */

// ----------------------------------------------------------------------------------------------------------

#ifndef OFDM_UL_MAP_IE_H
#define OFDM_UL_MAP_IE_H

#include "cid.h"

#include "ns3/header.h"

#include <stdint.h>

namespace ns3
{

/**
 * This class implements the UL-MAP_IE message as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 6.3.2.3.43.7 UL-MAP_IE, page 115
 */
class OfdmUlMapIe
{
  public:
    OfdmUlMapIe();
    ~OfdmUlMapIe();

    /**
     * Set CID
     * @param cid the CID
     */
    void SetCid(const Cid& cid);
    /**
     * Set start time
     * @param startTime the start time
     */
    void SetStartTime(uint16_t startTime);
    /**
     * Set subchannel index
     * @param subchannelIndex
     */
    void SetSubchannelIndex(uint8_t subchannelIndex);
    /**
     * Set UIUC
     * @param uiuc the UIUC
     */
    void SetUiuc(uint8_t uiuc);
    /**
     * Set duration
     * @param duration the duration
     */
    void SetDuration(uint16_t duration);
    /**
     * Set midambe repetition interval
     * @param midambleRepetitionInterval the midambe repetition interval
     */
    void SetMidambleRepetitionInterval(uint8_t midambleRepetitionInterval);

    /**
     * Get CID
     * @returns the CID
     */
    Cid GetCid() const;
    /**
     * Get start time
     * @returns the start time
     */
    uint16_t GetStartTime() const;
    /**
     * Get subchannel index
     * @returns the subchannel index
     */
    uint8_t GetSubchannelIndex() const;
    /**
     * Get UIUC
     * @returns the UIUC
     */
    uint8_t GetUiuc() const;
    /**
     * Get duration
     * @returns the duration
     */
    uint16_t GetDuration() const;
    /**
     * Get Midamble repetition interval
     * @returns the midamble repetition interval
     */
    uint8_t GetMidambleRepetitionInterval() const;

    /**
     * Get size
     * @returns the size
     */
    uint16_t GetSize() const;

    /**
     * Write item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Write(Buffer::Iterator start) const;
    /**
     * Read item
     * @param start the iterator
     * @returns the updated iterator
     */
    Buffer::Iterator Read(Buffer::Iterator start);

  private:
    Cid m_cid;                            ///< CID
    uint16_t m_startTime;                 ///< start time
    uint8_t m_subchannelIndex;            ///< subchannel index
    uint8_t m_uiuc;                       ///< UIUC
    uint16_t m_duration;                  ///< duration
    uint8_t m_midambleRepetitionInterval; ///< midamble repetition interval

    // fields to be implemented later on:
    // m_focusedContentionIe()
    // m_subchannelizedNetworkEntryIe()
    // m_ulExtendedIe()
    // m_paddingNibble;
};

} // namespace ns3

#endif /* OFDM_UL_MAP_IE_H */

// ----------------------------------------------------------------------------------------------------------

#ifndef UL_MAP_H
#define UL_MAP_H

#include "ns3/header.h"

#include <stdint.h>
#include <vector>

namespace ns3
{

/**
 * This class implements the UL-MAP_IE message as described by "IEEE Standard for
 * Local and metropolitan area networks Part 16: Air Interface for Fixed Broadband Wireless Access
 * Systems" 8.2.1.8.2 Compressed UL-MAP, page 404
 */
class UlMap : public Header
{
  public:
    UlMap();
    ~UlMap() override;

    /**
     * Set UCD count
     * @param ucdCount the UCD count
     */
    void SetUcdCount(uint8_t ucdCount);
    /**
     * Set allocation start time
     * @param allocationStartTime the allocation start time
     */
    void SetAllocationStartTime(uint32_t allocationStartTime);
    /**
     * Add UL map element
     * @param ulMapElement the UL map element
     */
    void AddUlMapElement(OfdmUlMapIe ulMapElement);

    /**
     * Get UCD count
     * @returns the UCD count
     */
    uint8_t GetUcdCount() const;
    /**
     * Get allocation start time
     * @returns the allocation start time
     */
    uint32_t GetAllocationStartTime() const;
    /**
     * Get UL map elements
     * @returns the UL map elements
     */
    std::list<OfdmUlMapIe> GetUlMapElements() const;

    /**
     * Get name
     * @returns the name
     */
    std::string GetName() const;
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint8_t m_reserved; ///< changed as per the amendment 802.16e-2005

    uint8_t m_ucdCount;                     ///< UCD count
    uint32_t m_allocationStartTime;         ///< allocation start time
    std::list<OfdmUlMapIe> m_ulMapElements; ///< UL map elements
};

} // namespace ns3

#endif /* UL_MAP_H */
