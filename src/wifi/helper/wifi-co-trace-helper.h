/*
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
 */

#ifndef WIFI_CO_HELPER_H
#define WIFI_CO_HELPER_H

#include "ns3/callback.h"
#include "ns3/config.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/wifi-phy-state.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace ns3
{

class WifiNetDevice;
class NodeContainer;
class NetDeviceContainer;

/**
 * Data structure to track durations in each WifiPhy state. Elements in m_linkStateDurations are
 * indexed by "linkId".
 */
struct DeviceRecord
{
    uint32_t m_nodeId;
    std::string m_nodeName;
    uint32_t m_ifIndex;
    std::string m_deviceName;
    DeviceRecord(Ptr<WifiNetDevice> device);
    void AddLinkMeasurement(size_t linkId, Time start, Time duration, WifiPhyState state);
    std::vector<std::map<WifiPhyState, Time>> m_linkStateDurations;
};

/**
 * @class WifiCoTraceHelper
 * @brief Track channel occupancy durations for WifiNetDevice WifiPhy objects
 *
 * The WifiCoTraceHelper class tracks the duration that a particular WifiPhy object is in different
 * states.  The states are defined by the ns-3 WifiPhyStateHelper and include states such as
 * IDLE, CCA_BUSY, TX, and RX.  The helper tracks these durations between a user-configured
 * start and end time.  At the end of a simulation, this helper can print out statistics
 * on channel occupancy, and permits the export of an internal data structure to allow for
 * custom printing or statistics handling.
 *
 * This helper supports both single-link devices and multi-link devices (MLD).
 */
class WifiCoTraceHelper
{
  public:
    /**
     * Default Constructor. It will measure at all times.
     */
    WifiCoTraceHelper();

    /**
    * Construct a helper object measuring between two simulation time points
    *
    startTime, stopTime)
    *
    * @param startTime The measurement start time
    * @param stopTime The measurement stop time
    * Default Constructor. It will measure at all time
    */
    WifiCoTraceHelper(Time startTime, Time stopTime);

    /**
     * Enables trace collection for all nodes and WifiNetDevices in the specified NodeContainer.
     * @param nodes The NodeContainer to which traces are to be connected.
     */
    void Enable(NodeContainer nodes);
    /**
     * Enables trace collection for all nodes corresponding to the devices in the specified
     * NetDeviceContainer.
     * @param devices The NetDeviceContainer containing nodes to which traces are to be connected.
     */
    void Enable(NetDeviceContainer devices);

    /**
     * Starts the collection of statistics from a specified start time.
     * @param startTime The time to start collecting statistics.
     */
    void Start(Time startTime);

    /**
     * Stops the collection of statistics at a specified time.
     * @param stopTime The time to stop collecting statistics.
     */
    void Stop(Time stopTime);

    /**
     * Resets the current statistics, clearing all Phy states and durations.
     */
    void Reset();

    /**
     * Print measurement results on an output stream
     *
     * @param os The output stream to print to
     */
    void PrintStatistics(std::ostream& os) const;

    /**
     * Returns measurement results on each installed device.
     *
     */
    const std::vector<DeviceRecord>& GetDeviceRecords() const;

  private:
    uint32_t m_numDevices{0};

    Time m_startTime;
    Time m_stopTime{Time::Max()};

    std::vector<DeviceRecord> m_deviceRecords;

    void NotifyWifiPhyState(std::size_t idx,
                            std::size_t linkId,
                            Time start,
                            Time duration,
                            WifiPhyState state);

    std::map<WifiPhyState, double> ComputePercentage(
        const std::map<WifiPhyState, Time>& linkStates) const;

    std::ostream& PrintLinkStates(std::ostream& os,
                                  const std::map<WifiPhyState, Time>& linkStates) const;

    /**
     * A helper function used by PrintLinkStates method. It pads each string at left with space
     * characters such that the decimal points are at the same position.
     */
    void alignDecimal(std::vector<std::string>& column) const;

    /**
     * A helper function used by PrintLinkStates method. It pads each string at right with space
     * characters such that all strings have same width.
     */
    void alignWidth(std::vector<std::string>& column) const;
};

} // namespace ns3

#endif
