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

#include "tcp-accecn-data.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpAccEcnData);

TypeId
TcpAccEcnData::GetTypeId (void)
{
  static TypeId tid = TypeId("ns3::TcpAccEcnData")
          .SetParent<Object>()
          .SetGroupName("Internet")
          .AddConstructor<TcpAccEcnData>()
          .AddTraceSource("CepS",
                          "For data sender the number of packets marked respectively with the CE",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnCepS),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("CebS",
                          "For data sender the number of TCP payload bytes in packets marked respectively with the CE",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnCebS),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("E0bS",
                          "For data sender the number of TCP payload bytes in packets marked respectively with the ECT(0)",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnE0bS),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("E1bS",
                          "For data sender the number of TCP payload bytes in packets marked respectively with the ECT(1)",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnE1bS),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("CepR",
                          "For data receiver the number of packets marked respectively with the CE",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnCepR),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("CebR",
                          "For data receiver the number of TCP payload bytes in packets marked respectively with the CE",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnCebR),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("E0bR",
                          "For data receiver the number of TCP payload bytes in packets marked respectively with the ECT(0)",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnE0bR),
                          "ns3::TracedValue::Uint32Callback")
          .AddTraceSource("E1bR",
                          "For data receiver the number of TCP payload bytes in packets marked respectively with the ECT(1)",
                          MakeTraceSourceAccessor(&TcpAccEcnData::m_ecnE1bR),
                          "ns3::TracedValue::Uint32Callback");
  return tid;
}
} 