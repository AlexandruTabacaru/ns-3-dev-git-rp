/*
 * Copyright (c) 2010 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella (tommaso.pecorella@unifi.it)
 * Author: Valerio Sartini (valesar@gmail.com)
 */

#ifndef INET_TOPOLOGY_READER_H
#define INET_TOPOLOGY_READER_H

#include "topology-reader.h"

/**
 * @file
 * @ingroup topology
 * ns3::InetTopologyReader declaration.
 */

namespace ns3
{

// ------------------------------------------------------------
// --------------------------------------------
/**
 * @ingroup topology
 *
 * @brief Topology file reader (Inet-format type).
 *
 * This class takes an input file in Inet format and extracts all
 * the information needed to build the topology
 * (i.e.number of nodes, links and links structure).
 * It have been tested with Inet 3.0
 * https://web.archive.org/web/20210308100536/http://topology.eecs.umich.edu/inet/
 *
 * It might set a link attribute named "Weight", corresponding to
 * the euclidean distance between two nodes, the nodes being randomly positioned.
 */
class InetTopologyReader : public TopologyReader
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId.
     */
    static TypeId GetTypeId();

    InetTopologyReader();
    ~InetTopologyReader() override;

    // Delete copy constructor and assignment operator to avoid misuse
    InetTopologyReader(const InetTopologyReader&) = delete;
    InetTopologyReader& operator=(const InetTopologyReader&) = delete;

    /**
     * @brief Main topology reading function.
     *
     * This method opens an input stream and reads the Inet-format file.
     * From the first line it takes the total number of nodes and links.
     * Then discards a number of rows equals to total nodes (containing
     * useless geographical information).
     * Then reads until the end of the file (total links number rows) and saves
     * the structure of every single link in the topology.
     *
     * @return The container of the nodes created (or empty container if there was an error)
     */
    NodeContainer Read() override;

    // end class InetTopologyReader
};

// end namespace ns3
}; // namespace ns3

#endif /* INET_TOPOLOGY_READER_H */
