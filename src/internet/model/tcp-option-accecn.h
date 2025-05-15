/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Tsinghua University
 * Copyright (c) 2018 NITK Surathkal
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
 * Authors: Wenying Dai <dwy927@gmail.com>
 *          Mohit P. Tahiliani <tahiliani.nitk@gmail.com>
 */

#ifndef TCP_OPTION_ACCECN_H
#define TCP_OPTION_ACCECN_H

#include "ns3/tcp-option.h"
#include "ns3/sequence-number.h"

namespace ns3 {

class TcpOptionAccEcn : public TcpOptionExperimental
{
public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);

    TcpOptionAccEcn ();
    virtual ~TcpOptionAccEcn ();

    virtual void Print (std::ostream &os) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual uint32_t GetSerializedSize (void) const;

    virtual uint16_t GetExID (void) const; //!< magic number is 0xACCE for AccEcn

    uint32_t GetE0B (void) const;
    uint32_t GetCEB (void) const;
    uint32_t GetE1B (void) const;
    void SetE0B (uint32_t e0b);
    void SetCEB (uint32_t ceb);
    void SetE1B (uint32_t e1b);

protected:
    uint32_t m_e0b; //!< the number of TCP payload bytes in packets marked respectively with the ECT(0)
    uint32_t m_ceb; //!< the number of TCP payload bytes in packets marked respectively with the CE
    uint32_t m_e1b; //!< the number of TCP payload bytes in packets marked respectively with the ECT(1)

};

} // namespace ns3

#endif /* TCP_OPTION_ACCECN */ 