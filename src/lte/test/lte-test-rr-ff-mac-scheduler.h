/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Marco Miozzo <marco.miozzo@cttc.es>
 */

#ifndef LENA_TEST_RR_FF_MAC_SCHEDULER_H
#define LENA_TEST_RR_FF_MAC_SCHEDULER_H

#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup lte-test
 *
 * @brief This system test program creates different test cases with a single eNB and
 * This system test program creates different test cases with a single eNB and
 * several UEs, all having the same Radio Bearer specification. In each test
 * case, the UEs see the same SINR from the eNB; different test cases are
 * implemented obtained by using different SINR values and different numbers of
 * UEs. The test consists on checking that the obtained throughput performance
 * is equal among users and matches a reference throughput value within a given
 * tolerance.
 */
class LenaRrFfMacSchedulerTestCase : public TestCase
{
  public:
    /**
     * Constructor
     *
     * @param nUser the number of UE nodes
     * @param dist the distance between nodes
     * @param thrRefDl the DL throughput reference
     * @param thrRefUl the UL throughput reference
     * @param errorModelEnabled if true the error model is enabled
     */
    LenaRrFfMacSchedulerTestCase(uint16_t nUser,
                                 double dist,
                                 double thrRefDl,
                                 double thrRefUl,
                                 bool errorModelEnabled);
    ~LenaRrFfMacSchedulerTestCase() override;

  private:
    void DoRun() override;
    /**
     * Builds the test name string based on provided parameter values
     * @param nUser the number of UE nodes
     * @param dist the distance between UE nodes and eNodeB
     * @returns the name string
     */
    static std::string BuildNameString(uint16_t nUser, double dist);
    uint16_t m_nUser;         ///< number of UE nodes
    double m_dist;            ///< the distance between nodes
    double m_thrRefDl;        ///< the DL throughput reference
    double m_thrRefUl;        ///< the UL throughput reference
    bool m_errorModelEnabled; ///< indicates whether the error model is enabled
};

/**
 * @ingroup lte-test
 *
 * @brief Test suite for RrFfMacScheduler test
 */
class LenaTestRrFfMacSchedulerSuite : public TestSuite
{
  public:
    LenaTestRrFfMacSchedulerSuite();
};

#endif /* LENA_TEST_RR_FF_MAC_SCHEDULER_H */
