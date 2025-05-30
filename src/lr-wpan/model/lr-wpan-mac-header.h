/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *  Author: kwong yin <kwong-sang.yin@boeing.com>
 */

/*
 * the following classes implements the 802.15.4 Mac Header
 * There are 4 types of 802.15.4 Mac Headers Frames, and they have these fields
 *
 *    Headers Frames  : Fields
 *    -------------------------------------------------------------------------------------------
 *    Beacon          : Frame Control, Sequence Number, Address Fields+, Auxiliary Security
 * Header++. Data            : Frame Control, Sequence Number, Address Fields++, Auxiliary Security
 * Header++. Acknowledgment  : Frame Control, Sequence Number. Command         : Frame Control,
 * Sequence Number, Address Fields++, Auxiliary Security Header++.
 *
 *    + - The Address fields in Beacon frame is made up of the Source PAN Id and address only and
 * size is  4 or 8 octets whereas the other frames may contain the Destination PAN Id and address as
 *        well. (see specs).
 *    ++ - These fields are optional and of variable size
 */

#ifndef LR_WPAN_MAC_HEADER_H
#define LR_WPAN_MAC_HEADER_H

#include "ns3/header.h"
#include "ns3/mac16-address.h"
#include "ns3/mac64-address.h"

namespace ns3
{
namespace lrwpan
{

/**
 * @ingroup lr-wpan
 * Represent the Mac Header with the Frame Control and Sequence Number fields
 */
class LrWpanMacHeader : public Header
{
  public:
    /**
     * The possible MAC types, see IEEE 802.15.4-2006, Table 79.
     */
    enum LrWpanMacType
    {
        LRWPAN_MAC_BEACON = 0,         //!< LRWPAN_MAC_BEACON
        LRWPAN_MAC_DATA = 1,           //!< LRWPAN_MAC_DATA
        LRWPAN_MAC_ACKNOWLEDGMENT = 2, //!< LRWPAN_MAC_ACKNOWLEDGMENT
        LRWPAN_MAC_COMMAND = 3,        //!< LRWPAN_MAC_COMMAND
        LRWPAN_MAC_RESERVED            //!< LRWPAN_MAC_RESERVED
    };

    /**
     * The addressing mode types, see IEEE 802.15.4-2006, Table 80.
     */
    enum AddrModeType
    {
        NOADDR = 0,
        RESADDR = 1,
        SHORTADDR = 2,
        EXTADDR = 3
    };

    /**
     * The addressing mode types, see IEEE 802.15.4-2006, Table 80.
     */
    enum KeyIdModeType
    {
        IMPLICIT = 0,
        NOKEYSOURCE = 1,
        SHORTKEYSOURCE = 2,
        LONGKEYSOURCE = 3
    };

    LrWpanMacHeader();

    /**
     * Constructor
     * @param wpanMacType the header MAC type
     * @param seqNum the sequence number
     *
     * @internal
     * Data, ACK, Control MAC Header must have frame control and sequence number.
     * Beacon MAC Header must have frame control, sequence number, source PAN Id, source address.
     */
    LrWpanMacHeader(LrWpanMacType wpanMacType, uint8_t seqNum);

    ~LrWpanMacHeader() override;

    /**
     * Get the header type
     * @return the header type
     */
    LrWpanMacType GetType() const;
    /**
     * Get the Frame control field
     * @return the Frame control field
     */
    uint16_t GetFrameControl() const;
    /**
     * Check if Security Enabled bit of Frame Control is enabled
     * @return true if Security Enabled bit is enabled
     */
    bool IsSecEnable() const;
    /**
     * Check if Frame Pending bit of Frame Control is enabled
     * @return true if Frame Pending bit is enabled
     */
    bool IsFrmPend() const;
    /**
     * Check if Ack. Request bit of Frame Control is enabled
     * @return true if Ack. Request bit is enabled
     */
    bool IsAckReq() const;
    /**
     * Check if PAN ID Compression bit of Frame Control is enabled
     * @return true if PAN ID Compression bit is enabled
     */
    bool IsPanIdComp() const;
    /**
     * Get the Reserved bits of Frame control field
     * @return the Reserved bits
     */
    uint8_t GetFrmCtrlRes() const;
    /**
     * Get the Dest. Addressing Mode of Frame control field
     * @return the Dest. Addressing Mode bits
     */
    uint8_t GetDstAddrMode() const;
    /**
     * Get the Frame Version of Frame control field
     * @return the Frame Version bits
     */
    uint8_t GetFrameVer() const;
    /**
     * Get the Source Addressing Mode of Frame control field
     * @return the Source Addressing Mode bits
     */
    uint8_t GetSrcAddrMode() const;
    /**
     * Get the frame Sequence number
     * @return the sequence number
     */
    uint8_t GetSeqNum() const;
    /**
     * Get the Destination PAN ID
     * @return the Destination PAN ID
     */
    uint16_t GetDstPanId() const;
    /**
     * Get the Destination Short address
     * @return the Destination Short address
     */
    Mac16Address GetShortDstAddr() const;
    /**
     * Get the Destination Extended address
     * @return the Destination Extended address
     */
    Mac64Address GetExtDstAddr() const;
    /**
     * Get the Source PAN ID
     * @return the Source PAN ID
     */
    uint16_t GetSrcPanId() const;
    /**
     * Get the Source Short address
     * @return the Source Short address
     */
    Mac16Address GetShortSrcAddr() const;
    /**
     * Get the Source Extended address
     * @return the Source Extended address
     */
    Mac64Address GetExtSrcAddr() const;
    /**
     * Get the Auxiliary Security Header - Security Control Octet
     * @return the Auxiliary Security Header - Security Control Octet
     */
    uint8_t GetSecControl() const;
    /**
     * Get the Auxiliary Security Header - Frame Counter Octets
     * @return the Auxiliary Security Header - Frame Counter Octets
     */
    uint32_t GetFrmCounter() const;

    /**
     * Get the Auxiliary Security Header - Security Control - Security Level bits
     * @return the Auxiliary Security Header - Security Control - Security Level bits
     */
    uint8_t GetSecLevel() const;
    /**
     * Get the Auxiliary Security Header - Security Control - Key Identifier Mode bits
     * @return the Auxiliary Security Header - Security Control - Key Identifier Mode bits
     */
    uint8_t GetKeyIdMode() const;
    /**
     * Get the Auxiliary Security Header - Security Control - Reserved bits
     * @return the Auxiliary Security Header - Security Control - Reserved bits
     */
    uint8_t GetSecCtrlReserved() const;
    /**
     * Get the Auxiliary Security Header - Key Identifier - Key Source (2 Octets)
     * @return the Auxiliary Security Header - Key Identifier - Key Source  (2 Octets)
     */
    uint32_t GetKeyIdSrc32() const;
    /**
     * Get the Auxiliary Security Header - Key Identifier - Key Source (4 Octets)
     * @return the Auxiliary Security Header - Key Identifier - Key Source  (4 Octets)
     */
    uint64_t GetKeyIdSrc64() const;
    /**
     * Get the Auxiliary Security Header - Key Identifier - Key Index
     * @return the Auxiliary Security Header - Key Identifier - Key Index
     */
    uint8_t GetKeyIdIndex() const;
    /**
     * Returns true if the header is a beacon
     * @return true if the header is a beacon
     */
    bool IsBeacon() const;
    /**
     * Returns true if the header is a data
     * @return true if the header is a data
     */
    bool IsData() const;
    /**
     * Returns true if the header is an ack
     * @return true if the header is an ack
     */
    bool IsAcknowledgment() const;
    /**
     * Returns true if the header is a command
     * @return true if the header is a command
     */
    bool IsCommand() const;
    /**
     * Set the Frame Control field "Frame Type" bits
     * @param wpanMacType the frame type
     */
    void SetType(LrWpanMacType wpanMacType);
    /**
     * Set the whole Frame Control field
     * @param frameControl the Frame Control field
     */
    void SetFrameControl(uint16_t frameControl);
    /**
     * Set the Frame Control field "Security Enabled" bit to true
     */
    void SetSecEnable();
    /**
     * Set the Frame Control field "Security Enabled" bit to false
     */
    void SetSecDisable();
    /**
     * Set the Frame Control field "Frame Pending" bit to true
     */
    void SetFrmPend();
    /**
     * Set the Frame Control field "Frame Pending" bit to false
     */
    void SetNoFrmPend();
    /**
     * Set the Frame Control field "Ack. Request" bit to true
     */
    void SetAckReq();
    /**
     * Set the Frame Control field "Ack. Request" bit to false
     */
    void SetNoAckReq();
    /**
     * Set the Frame Control field "PAN ID Compression" bit to true
     */
    void SetPanIdComp();
    /**
     * Set the Frame Control field "PAN ID Compression" bit to false
     */
    void SetNoPanIdComp();
    /**
     * Set the Frame Control field "Reserved" bits
     * @param res reserved bits
     */
    void SetFrmCtrlRes(uint8_t res);
    /**
     * Set the Destination address mode
     * @param addrMode Destination address mode
     */
    void SetDstAddrMode(uint8_t addrMode);
    /**
     * Set the Frame version
     * @param ver frame version
     */
    void SetFrameVer(uint8_t ver);
    /**
     * Set the Source address mode
     * @param addrMode Source address mode
     */
    void SetSrcAddrMode(uint8_t addrMode);
    /**
     * Set the Sequence number
     * @param seqNum sequence number
     */
    void SetSeqNum(uint8_t seqNum);
    /* The Source/Destination Addressing fields are only set if SrcAddrMode/DstAddrMode are set */
    /**
     * Set Source address fields
     * @param panId source PAN ID
     * @param addr source address (16 bit)
     */
    void SetSrcAddrFields(uint16_t panId, Mac16Address addr);
    /**
     * Set Source address fields
     * @param panId source PAN ID
     * @param addr source address (64 bit)
     */
    void SetSrcAddrFields(uint16_t panId, Mac64Address addr);
    /**
     * Set Destination address fields
     * @param panId destination PAN ID
     * @param addr destination address (16 bit)
     */
    void SetDstAddrFields(uint16_t panId, Mac16Address addr);
    /**
     * Set Destination address fields
     * @param panId destination PAN ID
     * @param addr destination address (64 bit)
     */
    void SetDstAddrFields(uint16_t panId, Mac64Address addr);

    /* Auxiliary Security Header is only set if Sec Enable (SecU) field is set to 1 */
    /**
     * Set the auxiliary security header "Security Control" octet
     * @param secLevel the "Security Control" octet
     */
    void SetSecControl(uint8_t secLevel);
    /**
     * Set the auxiliary security header "Frame Counter" octet
     * @param frmCntr the "Frame Counter" octet
     */
    void SetFrmCounter(uint32_t frmCntr);
    /**
     * Set the Security Control field "Security Level" bits (3 bits)
     * @param secLevel the "Security Level" bits
     */
    void SetSecLevel(uint8_t secLevel);
    /**
     * Set the Security Control field "Key Identifier Mode" bits (2 bits)
     * @param keyIdMode the "Key Identifier Mode" bits
     */
    void SetKeyIdMode(uint8_t keyIdMode);
    /**
     * Set the Security Control field "Reserved" bits (3 bits)
     * @param res the "Reserved" bits
     */
    void SetSecCtrlReserved(uint8_t res);
    /* Variable length will be dependent on Key Id Mode*/
    /**
     * Set the Key Index
     * @param keyIndex the Key index
     */
    void SetKeyId(uint8_t keyIndex);
    /**
     * Set the Key Index and originator
     * @param keySrc the originator of a group key
     * @param keyIndex the Key index
     */
    void SetKeyId(uint32_t keySrc, uint8_t keyIndex);
    /**
     * Set the Key Index and originator
     * @param keySrc the originator of a group key
     * @param keyIndex the Key index
     */
    void SetKeyId(uint64_t keySrc, uint8_t keyIndex);
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
    /* Frame Control 2 Octets */
    /* Frame Control field - see 7.2.1.1 */
    uint8_t m_fctrlFrmType; //!< Frame Control field Bit 0-2    = 0 - Beacon, 1 - Data, 2 - Ack, 3 -
                            //!< Command
    uint8_t m_fctrlSecU;    //!< Frame Control field Bit 3      = 0 - no AuxSecHdr ,  1 - security
                            //!< enabled AuxSecHdr present
    uint8_t m_fctrlFrmPending;  //!< Frame Control field Bit 4
    uint8_t m_fctrlAckReq;      //!< Frame Control field Bit 5
    uint8_t m_fctrlPanIdComp;   //!< Frame Control field Bit 6      = 0 - no compression, 1 - using
                                //!< only DstPanId for both Src and DstPanId
    uint8_t m_fctrlReserved;    //!< Frame Control field Bit 7-9
    uint8_t m_fctrlDstAddrMode; //!< Frame Control field Bit 10-11  = 0 - No DstAddr, 2 -
                                //!< ShtDstAddr, 3 - ExtDstAddr
    uint8_t m_fctrlFrmVer;      //!< Frame Control field Bit 12-13
    uint8_t m_fctrlSrcAddrMode; //!< Frame Control field Bit 14-15  = 0 - No SrcAddr, 2 -
                                //!< ShtSrcAddr, 3 - ExtSrcAddr

    /* Sequence Number */
    uint8_t m_SeqNum; //!< Sequence Number (1 Octet)

    /* Addressing fields */
    uint16_t m_addrDstPanId;         //!< Dst PAN id (0 or 2 Octets)
    Mac16Address m_addrShortDstAddr; //!< Dst Short addr (0 or 2 Octets)
    Mac64Address m_addrExtDstAddr;   //!< Dst Ext addr (0 or 8 Octets)
    uint16_t m_addrSrcPanId;         //!< Src PAN id (0 or 2 Octets)
    Mac16Address m_addrShortSrcAddr; //!< Src Short addr (0 or 2 Octets)
    Mac64Address m_addrExtSrcAddr;   //!< Src Ext addr (0 or 8 Octets)

    /* Auxiliary Security Header - See 7.6.2 - 0, 5, 6, 10 or 14 Octets */
    // uint8_t m_auxSecCtrl;              // 1 Octet see below
    uint32_t m_auxFrmCntr; //!< Auxiliary security header - Frame Counter (4 Octets)

    /* Security Control fields - See 7.6.2.2 */
    uint8_t m_secctrlSecLevel;  //!< Auxiliary security header - Security Control field - Bit 0-2
    uint8_t m_secctrlKeyIdMode; //!< Auxiliary security header - Security Control field - Bit 3-4
                                //!< will indicate size of Key Id
                                // = 0 - Key is determined implicitly
                                //       from originator and recipient
                                // = 1 - 1 Octet Key Index
                                // = 2 - 1 Octet Key Index and 4 oct Key src
                                // = 3 - 1 Octet Key Index and 8 oct Key src

    uint8_t m_secctrlReserved; //!< Auxiliary security header - Security Control field - Bit 5-7

    /// Auxiliary security header
    union {
        uint32_t m_auxKeyIdKeySrc32; //!< Auxiliary security header - Key Source (4 Octets)
        uint64_t m_auxKeyIdKeySrc64; //!< Auxiliary security header - Key Source (8 Octets)
    };

    uint8_t m_auxKeyIdKeyIndex; //!< Auxiliary security header - Key Index (1 Octet)

}; // LrWpanMacHeader

} // namespace lrwpan
} // namespace ns3

#endif /* LR_WPAN_MAC_HEADER_H */
