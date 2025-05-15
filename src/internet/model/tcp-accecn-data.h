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

#ifndef TCP_ACCECN_DATA_H
#define TCP_ACCECN_DATA_H

#include "ns3/object.h"
#include "ns3/traced-value.h"

namespace ns3 {

class TcpAccEcnData : public Object
{
public:

  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  // Default copy-constructor, destructor
  TcpAccEcnData () : Object (),
      m_ecnCepS (0), m_ecnCebS (0), m_ecnE0bS (0), m_ecnE1bS (0),
      m_ecnCepR (0), m_ecnCebR (0), m_ecnE0bR (0), m_ecnE1bR (0),
      m_useDelAckAccEcn (true), m_isIniS (false), m_isIniR (false)
  {};

  void IniSenderCounters () { if(!m_isIniS) { m_isIniS = true; m_ecnCepS = 5; m_ecnE0bS = 1; m_ecnCebS = 0; m_ecnE1bS = 0;} }
  void IniReceiverCounters () { if(!m_isIniR) { m_isIniR = true; m_ecnCepR = 5; m_ecnE0bR = 1; m_ecnCebR = 0; m_ecnE1bR = 0;} }

  TracedValue<uint32_t>   m_ecnCepS    {0}; //!< For data sender, the number of packets marked respectively with the CE
  TracedValue<uint32_t>   m_ecnCebS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the CE
  TracedValue<uint32_t>   m_ecnE0bS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the ECT(0)
  TracedValue<uint32_t>   m_ecnE1bS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the ECT(1)
  TracedValue<uint32_t>   m_ecnCepR    {0}; //!< For data receiver, the number of packets marked respectively with the CE
  TracedValue<uint32_t>   m_ecnCebR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the CE
  TracedValue<uint32_t>   m_ecnE0bR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the ECT(0)
  TracedValue<uint32_t>   m_ecnE1bR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the ECT(1)
  bool m_useDelAckAccEcn  {true};

private:
  bool       m_isIniS     {false};
  bool       m_isIniR     {false};
};
} // namespace ns3

#endif //TCP_ACCECN_DATA_H 