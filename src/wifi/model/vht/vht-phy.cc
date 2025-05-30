/*
 * Copyright (c) 2020 Orange Labs
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Rediet <getachew.redieteab@orange.com>
 *          Sébastien Deronne <sebastien.deronne@gmail.com> (for logic ported from wifi-phy)
 */

#include "vht-phy.h"

#include "vht-configuration.h"
#include "vht-ppdu.h"

#include "ns3/assert.h"
#include "ns3/interference-helper.h"
#include "ns3/log.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h" //only used for static mode constructor
#include "ns3/wifi-psdu.h"
#include "ns3/wifi-utils.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT WIFI_PHY_NS_LOG_APPEND_CONTEXT(m_wifiPhy)

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("VhtPhy");

/*******************************************************
 *       VHT PHY (IEEE 802.11-2016, clause 21)
 *******************************************************/

// clang-format off

const PhyEntity::PpduFormats VhtPhy::m_vhtPpduFormats {
    { WIFI_PREAMBLE_VHT_SU, { WIFI_PPDU_FIELD_PREAMBLE,      // L-STF + L-LTF
                              WIFI_PPDU_FIELD_NON_HT_HEADER, // L-SIG
                              WIFI_PPDU_FIELD_SIG_A,         // VHT-SIG-A
                              WIFI_PPDU_FIELD_TRAINING,      // VHT-STF + VHT-LTFs
                              WIFI_PPDU_FIELD_DATA } },
    { WIFI_PREAMBLE_VHT_MU, { WIFI_PPDU_FIELD_PREAMBLE,      // L-STF + L-LTF
                              WIFI_PPDU_FIELD_NON_HT_HEADER, // L-SIG
                              WIFI_PPDU_FIELD_SIG_A,         // VHT-SIG-A
                              WIFI_PPDU_FIELD_TRAINING,      // VHT-STF + VHT-LTFs
                              WIFI_PPDU_FIELD_SIG_B,         // VHT-SIG-B
                              WIFI_PPDU_FIELD_DATA } }
};

const VhtPhy::NesExceptionMap VhtPhy::m_exceptionsMap {
                    /* {BW,Nss,MCS} Nes */
    { std::make_tuple (MHz_u{80}, 7, 2),  3 }, // instead of 2
    { std::make_tuple (MHz_u{80}, 7, 7),  6 }, // instead of 4
    { std::make_tuple (MHz_u{80}, 7, 8),  6 }, // instead of 5
    { std::make_tuple (MHz_u{80}, 8, 7),  6 }, // instead of 5
    { std::make_tuple (MHz_u{160}, 4, 7),  6 }, // instead of 5
    { std::make_tuple (MHz_u{160}, 5, 8),  8 }, // instead of 7
    { std::make_tuple (MHz_u{160}, 6, 7),  8 }, // instead of 7
    { std::make_tuple (MHz_u{160}, 7, 3),  4 }, // instead of 3
    { std::make_tuple (MHz_u{160}, 7, 4),  6 }, // instead of 5
    { std::make_tuple (MHz_u{160}, 7, 5),  7 }, // instead of 6
    { std::make_tuple (MHz_u{160}, 7, 7),  9 }, // instead of 8
    { std::make_tuple (MHz_u{160}, 7, 8), 12 }, // instead of 9
    { std::make_tuple (MHz_u{160}, 7, 9), 12 }, // instead of 10
};

// clang-format on

/**
 * @brief map a given channel list type to the corresponding scaling factor
 */
const std::map<WifiChannelListType, dBm_u> channelTypeToScalingFactor{
    {WIFI_CHANLIST_PRIMARY, dBm_u{0.0}},
    {WIFI_CHANLIST_SECONDARY, dBm_u{0.0}},
    {WIFI_CHANLIST_SECONDARY40, dBm_u{3.0}},
    {WIFI_CHANLIST_SECONDARY80, dBm_u{6.0}},
};

/**
 * @brief map a given secondary channel width to its channel list type
 */
const std::map<MHz_u, WifiChannelListType> vhtSecondaryChannels{
    {MHz_u{20}, WIFI_CHANLIST_SECONDARY},
    {MHz_u{40}, WIFI_CHANLIST_SECONDARY40},
    {MHz_u{80}, WIFI_CHANLIST_SECONDARY80},
};

VhtPhy::VhtPhy(bool buildModeList /* = true */)
    : HtPhy(1, false) // don't add HT modes to list
{
    NS_LOG_FUNCTION(this << buildModeList);
    m_bssMembershipSelector = VHT_PHY;
    m_maxMcsIndexPerSs = 9;
    m_maxSupportedMcsIndexPerSs = m_maxMcsIndexPerSs;
    if (buildModeList)
    {
        BuildModeList();
    }
}

VhtPhy::~VhtPhy()
{
    NS_LOG_FUNCTION(this);
}

void
VhtPhy::BuildModeList()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_modeList.empty());
    NS_ASSERT(m_bssMembershipSelector == VHT_PHY);
    for (uint8_t index = 0; index <= m_maxSupportedMcsIndexPerSs; ++index)
    {
        NS_LOG_LOGIC("Add VhtMcs" << +index << " to list");
        m_modeList.emplace_back(CreateVhtMcs(index));
    }
}

const PhyEntity::PpduFormats&
VhtPhy::GetPpduFormats() const
{
    return m_vhtPpduFormats;
}

WifiMode
VhtPhy::GetSigMode(WifiPpduField field, const WifiTxVector& txVector) const
{
    switch (field)
    {
    case WIFI_PPDU_FIELD_TRAINING: // consider SIG-A mode for training (useful for
                                   // InterferenceHelper)
    case WIFI_PPDU_FIELD_SIG_A:
        return GetSigAMode();
    case WIFI_PPDU_FIELD_SIG_B:
        return GetSigBMode(txVector);
    default:
        return HtPhy::GetSigMode(field, txVector);
    }
}

WifiMode
VhtPhy::GetHtSigMode() const
{
    NS_ASSERT(m_bssMembershipSelector != HT_PHY);
    NS_FATAL_ERROR("No HT-SIG");
    return WifiMode();
}

WifiMode
VhtPhy::GetSigAMode() const
{
    return GetLSigMode(); // same number of data tones as OFDM (i.e. 48)
}

WifiMode
VhtPhy::GetSigBMode(const WifiTxVector& txVector) const
{
    NS_ABORT_MSG_IF(txVector.GetPreambleType() != WIFI_PREAMBLE_VHT_MU,
                    "VHT-SIG-B only available for VHT MU");
    return GetVhtMcs0();
}

Time
VhtPhy::GetDuration(WifiPpduField field, const WifiTxVector& txVector) const
{
    switch (field)
    {
    case WIFI_PPDU_FIELD_SIG_A:
        return GetSigADuration(txVector.GetPreambleType());
    case WIFI_PPDU_FIELD_SIG_B:
        return GetSigBDuration(txVector);
    default:
        return HtPhy::GetDuration(field, txVector);
    }
}

Time
VhtPhy::GetLSigDuration(WifiPreamble /* preamble */) const
{
    return MicroSeconds(4); // L-SIG
}

Time
VhtPhy::GetHtSigDuration() const
{
    return MicroSeconds(0); // no HT-SIG
}

Time
VhtPhy::GetTrainingDuration(const WifiTxVector& txVector,
                            uint8_t nDataLtf,
                            uint8_t nExtensionLtf /* = 0 */) const
{
    NS_ABORT_MSG_IF(nDataLtf > 8, "Unsupported number of LTFs " << +nDataLtf << " for VHT");
    NS_ABORT_MSG_IF(nExtensionLtf > 0, "No extension LTFs expected for VHT");
    return MicroSeconds(4 + 4 * nDataLtf); // VHT-STF + VHT-LTFs
}

Time
VhtPhy::GetSigADuration(WifiPreamble /* preamble */) const
{
    return MicroSeconds(8); // VHT-SIG-A (first and second symbol)
}

Time
VhtPhy::GetSigBDuration(const WifiTxVector& txVector) const
{
    return (txVector.GetPreambleType() == WIFI_PREAMBLE_VHT_MU)
               ? MicroSeconds(4)
               : MicroSeconds(0); // HE-SIG-B only for MU
}

uint8_t
VhtPhy::GetNumberBccEncoders(const WifiTxVector& txVector) const
{
    WifiMode payloadMode = txVector.GetMode();
    /**
     * General rule: add an encoder when crossing maxRatePerCoder frontier
     *
     * The value of 540 Mbps and 600 Mbps for normal GI and short GI (resp.)
     * were obtained by observing the rates for which Nes was incremented in tables
     * 21-30 to 21-61 of IEEE 802.11-2016.
     * These values are the last values before changing encoders.
     */
    double maxRatePerCoder = (txVector.GetGuardInterval().GetNanoSeconds() == 800) ? 540e6 : 600e6;
    uint8_t nes = ceil(payloadMode.GetDataRate(txVector) / maxRatePerCoder);

    // Handle exceptions to the rule
    auto iter = m_exceptionsMap.find(
        std::make_tuple(txVector.GetChannelWidth(), txVector.GetNss(), payloadMode.GetMcsValue()));
    if (iter != m_exceptionsMap.end())
    {
        nes = iter->second;
    }
    return nes;
}

Ptr<WifiPpdu>
VhtPhy::BuildPpdu(const WifiConstPsduMap& psdus, const WifiTxVector& txVector, Time ppduDuration)
{
    NS_LOG_FUNCTION(this << psdus << txVector << ppduDuration);
    return Create<VhtPpdu>(psdus.begin()->second,
                           txVector,
                           m_wifiPhy->GetOperatingChannel(),
                           ppduDuration,
                           ObtainNextUid(txVector));
}

PhyEntity::PhyFieldRxStatus
VhtPhy::DoEndReceiveField(WifiPpduField field, Ptr<Event> event)
{
    NS_LOG_FUNCTION(this << field << *event);
    switch (field)
    {
    case WIFI_PPDU_FIELD_SIG_A:
        [[fallthrough]];
    case WIFI_PPDU_FIELD_SIG_B:
        return EndReceiveSig(event, field);
    default:
        return HtPhy::DoEndReceiveField(field, event);
    }
}

PhyEntity::PhyFieldRxStatus
VhtPhy::EndReceiveSig(Ptr<Event> event, WifiPpduField field)
{
    NS_LOG_FUNCTION(this << *event << field);
    SnrPer snrPer = GetPhyHeaderSnrPer(field, event);
    NS_LOG_DEBUG(field << ": SNR(dB)=" << RatioToDb(snrPer.snr) << ", PER=" << snrPer.per);
    PhyFieldRxStatus status(GetRandomValue() > snrPer.per);
    if (status.isSuccess)
    {
        NS_LOG_DEBUG("Received " << field);
        if (!IsAllConfigSupported(WIFI_PPDU_FIELD_SIG_A, event->GetPpdu()))
        {
            status = PhyFieldRxStatus(false, UNSUPPORTED_SETTINGS, DROP);
        }
        status = ProcessSig(event, status, field);
    }
    else
    {
        NS_LOG_DEBUG("Drop packet because " << field << " reception failed");
        status.reason = GetFailureReason(field);
        status.actionIfFailure = DROP;
    }
    return status;
}

WifiPhyRxfailureReason
VhtPhy::GetFailureReason(WifiPpduField field) const
{
    switch (field)
    {
    case WIFI_PPDU_FIELD_SIG_A:
        return SIG_A_FAILURE;
    case WIFI_PPDU_FIELD_SIG_B:
        return SIG_B_FAILURE;
    default:
        NS_ASSERT_MSG(false, "Unknown PPDU field");
        return UNKNOWN;
    }
}

PhyEntity::PhyFieldRxStatus
VhtPhy::ProcessSig(Ptr<Event> event, PhyFieldRxStatus status, WifiPpduField field)
{
    NS_LOG_FUNCTION(this << *event << status << field);
    NS_ASSERT(event->GetPpdu()->GetTxVector().GetPreambleType() >= WIFI_PREAMBLE_VHT_SU);
    // TODO see if something should be done here once MU-MIMO is supported
    return status; // nothing special for VHT
}

bool
VhtPhy::IsAllConfigSupported(WifiPpduField field, Ptr<const WifiPpdu> ppdu) const
{
    if (ppdu->GetType() == WIFI_PPDU_TYPE_DL_MU && field == WIFI_PPDU_FIELD_SIG_A)
    {
        return IsChannelWidthSupported(ppdu); // perform the full check after SIG-B
    }
    return HtPhy::IsAllConfigSupported(field, ppdu);
}

void
VhtPhy::InitializeModes()
{
    for (uint8_t i = 0; i < 10; ++i)
    {
        GetVhtMcs(i);
    }
}

WifiMode
VhtPhy::GetVhtMcs(uint8_t index)
{
#define CASE(x)                                                                                    \
    case x:                                                                                        \
        return GetVhtMcs##x();

    switch (index)
    {
        CASE(0)
        CASE(1)
        CASE(2)
        CASE(3)
        CASE(4)
        CASE(5)
        CASE(6)
        CASE(7)
        CASE(8)
        CASE(9)
    default:
        NS_ABORT_MSG("Inexistent index (" << +index << ") requested for VHT");
        return WifiMode();
    }
#undef CASE
}

#define GET_VHT_MCS(x)                                                                             \
    WifiMode VhtPhy::GetVhtMcs##x()                                                                \
    {                                                                                              \
        static WifiMode mcs = CreateVhtMcs(x);                                                     \
        return mcs;                                                                                \
    }

GET_VHT_MCS(0)
GET_VHT_MCS(1)
GET_VHT_MCS(2)
GET_VHT_MCS(3)
GET_VHT_MCS(4)
GET_VHT_MCS(5)
GET_VHT_MCS(6)
GET_VHT_MCS(7)
GET_VHT_MCS(8)
GET_VHT_MCS(9)
#undef GET_VHT_MCS

WifiMode
VhtPhy::CreateVhtMcs(uint8_t index)
{
    NS_ASSERT_MSG(index <= 9, "VhtMcs index must be <= 9!");
    return WifiModeFactory::CreateWifiMcs("VhtMcs" + std::to_string(index),
                                          index,
                                          WIFI_MOD_CLASS_VHT,
                                          false,
                                          MakeBoundCallback(&GetCodeRate, index),
                                          MakeBoundCallback(&GetConstellationSize, index),
                                          MakeCallback(&GetPhyRateFromTxVector),
                                          MakeCallback(&GetDataRateFromTxVector),
                                          MakeBoundCallback(&GetNonHtReferenceRate, index),
                                          MakeCallback(&IsAllowed));
}

WifiCodeRate
VhtPhy::GetCodeRate(uint8_t mcsValue)
{
    switch (mcsValue)
    {
    case 8:
        return WIFI_CODE_RATE_3_4;
    case 9:
        return WIFI_CODE_RATE_5_6;
    default:
        return HtPhy::GetCodeRate(mcsValue);
    }
}

uint16_t
VhtPhy::GetConstellationSize(uint8_t mcsValue)
{
    switch (mcsValue)
    {
    case 8:
    case 9:
        return 256;
    default:
        return HtPhy::GetConstellationSize(mcsValue);
    }
}

uint64_t
VhtPhy::GetPhyRate(uint8_t mcsValue, MHz_u channelWidth, Time guardInterval, uint8_t nss)
{
    WifiCodeRate codeRate = GetCodeRate(mcsValue);
    uint64_t dataRate = GetDataRate(mcsValue, channelWidth, guardInterval, nss);
    return HtPhy::CalculatePhyRate(codeRate, dataRate);
}

uint64_t
VhtPhy::GetPhyRateFromTxVector(const WifiTxVector& txVector, uint16_t /* staId */)
{
    return GetPhyRate(txVector.GetMode().GetMcsValue(),
                      txVector.GetChannelWidth(),
                      txVector.GetGuardInterval(),
                      txVector.GetNss());
}

uint64_t
VhtPhy::GetDataRateFromTxVector(const WifiTxVector& txVector, uint16_t /* staId */)
{
    return GetDataRate(txVector.GetMode().GetMcsValue(),
                       txVector.GetChannelWidth(),
                       txVector.GetGuardInterval(),
                       txVector.GetNss());
}

uint64_t
VhtPhy::GetDataRate(uint8_t mcsValue, MHz_u channelWidth, Time guardInterval, uint8_t nss)
{
    [[maybe_unused]] const auto gi = guardInterval.GetNanoSeconds();
    NS_ASSERT((gi == 800) || (gi == 400));
    NS_ASSERT(nss <= 8);
    NS_ASSERT_MSG(IsCombinationAllowed(mcsValue, channelWidth, nss),
                  "VHT MCS " << +mcsValue << " forbidden at " << channelWidth << " MHz when NSS is "
                             << +nss);
    return HtPhy::CalculateDataRate(GetSymbolDuration(guardInterval),
                                    GetUsableSubcarriers(channelWidth),
                                    static_cast<uint16_t>(log2(GetConstellationSize(mcsValue))),
                                    HtPhy::GetCodeRatio(GetCodeRate(mcsValue)),
                                    nss);
}

uint16_t
VhtPhy::GetUsableSubcarriers(MHz_u channelWidth)
{
    switch (static_cast<uint16_t>(channelWidth))
    {
    case 80:
        return 234;
    case 160:
        return 468;
    default:
        return HtPhy::GetUsableSubcarriers(channelWidth);
    }
}

uint64_t
VhtPhy::GetNonHtReferenceRate(uint8_t mcsValue)
{
    const auto codeRate = GetCodeRate(mcsValue);
    const auto constellationSize = GetConstellationSize(mcsValue);
    return CalculateNonHtReferenceRate(codeRate, constellationSize);
}

uint64_t
VhtPhy::CalculateNonHtReferenceRate(WifiCodeRate codeRate, uint16_t constellationSize)
{
    uint64_t dataRate;
    switch (constellationSize)
    {
    case 256:
        if (codeRate == WIFI_CODE_RATE_3_4 || codeRate == WIFI_CODE_RATE_5_6)
        {
            dataRate = 54000000;
        }
        else
        {
            NS_FATAL_ERROR("Trying to get reference rate for a MCS with wrong combination of "
                           "coding rate and modulation");
        }
        break;
    default:
        dataRate = HtPhy::CalculateNonHtReferenceRate(codeRate, constellationSize);
    }
    return dataRate;
}

bool
VhtPhy::IsAllowed(const WifiTxVector& txVector)
{
    return IsCombinationAllowed(txVector.GetMode().GetMcsValue(),
                                txVector.GetChannelWidth(),
                                txVector.GetNss());
}

bool
VhtPhy::IsCombinationAllowed(uint8_t mcsValue, MHz_u channelWidth, uint8_t nss)
{
    if (mcsValue == 9 && channelWidth == MHz_u{20} && nss != 3)
    {
        return false;
    }
    if (mcsValue == 6 && channelWidth == MHz_u{80} && nss == 3)
    {
        return false;
    }
    return true;
}

uint32_t
VhtPhy::GetMaxPsduSize() const
{
    return 4692480;
}

dBm_u
VhtPhy::GetCcaThreshold(const Ptr<const WifiPpdu> ppdu, WifiChannelListType channelType) const
{
    if (ppdu)
    {
        const auto ppduBw = ppdu->GetTxVector().GetChannelWidth();
        switch (channelType)
        {
        case WIFI_CHANLIST_PRIMARY: {
            // Start of a PPDU for which its power measured within the primary 20 MHz channel is at
            // or above the CCA sensitivity threshold.
            return m_wifiPhy->GetCcaSensitivityThreshold();
        }
        case WIFI_CHANLIST_SECONDARY:
            NS_ASSERT_MSG(ppduBw == MHz_u{20}, "Invalid channel width " << ppduBw);
            break;
        case WIFI_CHANLIST_SECONDARY40:
            NS_ASSERT_MSG(ppduBw <= MHz_u{40}, "Invalid channel width " << ppduBw);
            break;
        case WIFI_CHANLIST_SECONDARY80:
            NS_ASSERT_MSG(ppduBw <= MHz_u{80}, "Invalid channel width " << ppduBw);
            break;
        default:
            NS_ASSERT_MSG(false, "Invalid channel list type");
        }
        auto vhtConfiguration = m_wifiPhy->GetDevice()->GetVhtConfiguration();
        NS_ASSERT(vhtConfiguration);
        const auto thresholds = vhtConfiguration->GetSecondaryCcaSensitivityThresholdsPerBw();
        const auto it = thresholds.find(ppduBw);
        NS_ASSERT_MSG(it != std::end(thresholds), "Invalid channel width " << ppduBw);
        return it->second;
    }
    else
    {
        const auto it = channelTypeToScalingFactor.find(channelType);
        NS_ASSERT_MSG(it != std::end(channelTypeToScalingFactor), "Invalid channel list type");
        return m_wifiPhy->GetCcaEdThreshold() + it->second;
    }
}

const std::map<MHz_u, WifiChannelListType>&
VhtPhy::GetCcaSecondaryChannels() const
{
    return vhtSecondaryChannels;
}

} // namespace ns3

namespace
{

/**
 * Constructor class for VHT modes
 */
class ConstructorVht
{
  public:
    ConstructorVht()
    {
        ns3::VhtPhy::InitializeModes();
        ns3::WifiPhy::AddStaticPhyEntity(ns3::WIFI_MOD_CLASS_VHT, ns3::Create<ns3::VhtPhy>());
    }
} g_constructor_vht; ///< the constructor for VHT modes

} // namespace
