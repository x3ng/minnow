#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  debug( "add_route() called" );
  if (prefix_length > 32) {
    return;
  }
  TrieNode* ctn = trie_root_.get();
  uint32_t srp = route_prefix;
  const uint32_t MASK = 1U << 31;
  for (int p=0; p<prefix_length; ++p) {
    bool c = (srp & MASK) == MASK;
    if (!ctn->child[c]) {
      ctn->child[c] = std::make_unique<TrieNode>();
    }
    ctn = ctn->child[c].get();
    srp <<= 1;
  }
  ctn->next_hop = next_hop;
  ctn->interface_num = interface_num;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  //debug( "route() called" );
  const uint32_t MASK = 1U << 31;
  for (auto& itf: interfaces_) {
    auto& recv_dats = itf->datagrams_received();
    while (recv_dats.size()) {
      auto crd = recv_dats.front();
      recv_dats.pop();
      uint8_t& ttl = crd.header.ttl; 
      if (ttl <= 1) {
        continue;
      }
      uint32_t dst_ip = crd.header.dst;
      int p = 0;
      TrieNode* ctn = trie_root_.get();
      TrieNode* lmn = nullptr;
      while (p++<32 && ctn) {
        lmn = ctn->interface_num.has_value() ? ctn : lmn;
        bool cp = (dst_ip & MASK) == MASK;
        ctn = ctn->child[cp].get();
        dst_ip <<= 1;
      }
      if (lmn) {
        auto tif = interface(lmn->interface_num.value());
        if (!tif) {
          continue;
        }
        Address nh;
        if (lmn->next_hop.has_value()) {
          nh = lmn->next_hop.value();
        } else {
          nh = Address::from_ipv4_numeric(crd.header.dst);
        }
        ttl--;
        crd.header.compute_checksum();
        tif->send_datagram(crd, nh);
      }
    }
  }
}
