#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  debug( "send_datagram called" );
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();
  if (_arp_table.contains(next_hop_ip)) {
    EthernetAddress dst_ethernet_addr = _arp_table[next_hop_ip].ethernet_address;
    Serializer sl;
    dgram.serialize(sl);
    EthernetFrame ef;
    ef.header.dst = dst_ethernet_addr;
    ef.header.src = ethernet_address_;
    ef.header.type = EthernetHeader::TYPE_IPv4;
    ef.payload = sl.finish();
    transmit(ef);
  } else {
    auto it = _pending_datagrams.find(next_hop_ip);
    if (it != _pending_datagrams.end()) {
      it->second.datagrams.push_back(std::move(dgram));
      return;
    }
    ARPMessage arp_req;
    arp_req.opcode = ARPMessage::OPCODE_REQUEST;
    arp_req.sender_ethernet_address = ethernet_address_;
    arp_req.sender_ip_address = ip_address_.ipv4_numeric();
    arp_req.target_ethernet_address = {};
    arp_req.target_ip_address = next_hop_ip;
    Serializer arp_sl;
    arp_req.serialize(arp_sl);
    EthernetFrame ef;
    ef.header.dst = ETHERNET_BROADCAST;
    ef.header.src = ethernet_address_;
    ef.header.type = EthernetHeader::TYPE_ARP;
    ef.payload = arp_sl.finish();
    transmit(ef);
    PendingEntry pe;
    pe.datagrams.push_back(dgram);
    _pending_datagrams[next_hop_ip] = std::move(pe);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  debug( "recv_frame called" );
  if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    return;
  }
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram id;
    if (parse<InternetDatagram>(id, frame.payload)) {
      datagrams_received_.push(id);
    }
  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage am;
    if (parse<ARPMessage>(am, frame.payload)) {
      _arp_table[am.sender_ip_address] = ARPEntry(am.sender_ethernet_address, 0);
      if (am.opcode == ARPMessage::OPCODE_REQUEST && am.target_ip_address == ip_address_.ipv4_numeric()) {
        ARPMessage arp_rep;
        arp_rep.opcode = ARPMessage::OPCODE_REPLY;
        arp_rep.sender_ethernet_address = ethernet_address_;
        arp_rep.sender_ip_address = ip_address_.ipv4_numeric();
        arp_rep.target_ethernet_address = am.sender_ethernet_address;
        arp_rep.target_ip_address = am.sender_ip_address;
        Serializer arp_sl;
        arp_rep.serialize(arp_sl);
        EthernetFrame ef;
        ef.header.dst = am.sender_ethernet_address;
        ef.header.src = ethernet_address_;
        ef.header.type = EthernetHeader::TYPE_ARP;
        ef.payload = arp_sl.finish();
        transmit(ef);
      }
      auto it = _pending_datagrams.find(am.sender_ip_address);
      if (it != _pending_datagrams.end()) {
        for (auto& pd: it->second.datagrams) {
            send_datagram(pd, Address::from_ipv4_numeric(am.sender_ip_address));
        }
        _pending_datagrams.erase(it);
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "tick({}) called", ms_since_last_tick );
  for (auto it=_arp_table.begin(); it!=_arp_table.end(); ) {
    it->second.time_since_last_tick += ms_since_last_tick;
    if (it->second.time_since_last_tick > 30000) {
      it = _arp_table.erase(it);
    } else {
      it++;
    }
  }
  for (auto it=_pending_datagrams.begin(); it!=_pending_datagrams.end(); ) {
    it->second.arp_time_out += ms_since_last_tick;
    if (it->second.arp_time_out > 5000) {
      it = _pending_datagrams.erase(it);
    } else {
      it++;
    }
  }
}
