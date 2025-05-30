/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gary Pei <guangyu.pei@boeing.com>
 */
#ifndef LR_WPAN_SPECTRUM_VALUE_HELPER_H
#define LR_WPAN_SPECTRUM_VALUE_HELPER_H

#include "ns3/ptr.h"

namespace ns3
{

class SpectrumValue;

namespace lrwpan
{

/**
 * @ingroup lr-wpan
 *
 * @brief This class defines all functions to create spectrum model for LrWpan
 */
class LrWpanSpectrumValueHelper
{
  public:
    LrWpanSpectrumValueHelper();
    virtual ~LrWpanSpectrumValueHelper();

    /**
     * @brief create spectrum value
     * @param txPower the power transmission in dBm
     * @param channel the channel number per IEEE802.15.4
     * @return a Ptr to a newly created SpectrumValue instance
     */
    Ptr<SpectrumValue> CreateTxPowerSpectralDensity(double txPower, uint32_t channel);

    /**
     * @brief create spectrum value for noise
     * @param channel the channel number per IEEE802.15.4
     * @return a Ptr to a newly created SpectrumValue instance
     */
    Ptr<SpectrumValue> CreateNoisePowerSpectralDensity(uint32_t channel);

    /**
     * Set the noise factor added to the thermal noise.
     * @param f A dimensionless ratio (i.e. Not in dB)
     */
    void SetNoiseFactor(double f);

    /**
     * @brief total average power of the signal is the integral of the PSD using
     * the limits of the given channel
     * @param psd spectral density
     * @param channel the channel number per IEEE802.15.4
     * @return total power (using composite trap. rule to numerally integrate)
     */
    static double TotalAvgPower(Ptr<const SpectrumValue> psd, uint32_t channel);

  private:
    /**
     * A scaling factor for the noise power.
     * It specifies how much additional noise the device
     * contribute to the thermal noise (floor noise).
     */
    double m_noiseFactor;
};

} // namespace lrwpan
} // namespace ns3

#endif /*  LR_WPAN_SPECTRUM_VALUE_HELPER_H */
