#include "pkt.h"
#include "assume.h"

#include <byteswap.h>
#include <stdbool.h>
#include <string.h>

#define MAX(a, b) ((a)<(b) ? (b) : (a))

static const size_t proc4_req_sz = MAX(
    sizeof(eth_no_vlan_t)     + sizeof(ip_hdr_t), // when no vlan
    sizeof(eth_single_vlan_t) + sizeof(ip_hdr_t)  // when single vlan
  );

// loads all four possibly needed fields, then returns the needed value
// must have enough sz to do this, else it would be UB (reading off into
// potentially uninitialized memory)
static inline uint8_t proc4(char const* pkt, size_t sz)
{
  (void)sz;
  eth_no_vlan_t      no_vlan_hdr[1];
  ip_hdr_t           no_vlan_ip[1];

  eth_single_vlan_t single_vlan_hdr[1];
  ip_hdr_t          single_vlan_ip[1];

  // follow aliasing rules, load these all "speculatively"
  memcpy(no_vlan_hdr,     pkt,                             sizeof(eth_no_vlan_t));
  memcpy(no_vlan_ip,      pkt + sizeof(eth_no_vlan_t),     sizeof(ip_hdr_t));
  memcpy(single_vlan_hdr, pkt,                             sizeof(eth_single_vlan_t));
  memcpy(single_vlan_ip,  pkt + sizeof(eth_single_vlan_t), sizeof(ip_hdr_t));

  // cases: (vlan,non-vlan-ip,yes-vlan-ip)
  // 00x) non-vlan,     non-ip, (return 0)
  // 01x) non-vlan,     ip      (return proto)
  // 1x0) single-vlan,  non-ip  (return 0)
  // 1x1) single-vlan,  ip      (return proto)
  //
  // anything with and x is undefined, either case could be true
  // need 3 bits, so 8 values

  // the moral of the story is, we have to know if vlan to know if we can return

  uint8_t possible[8];

  // no vlan tag, not ip (the value for yes-vlan yes-ip is ignored)
  possible[ 0b000 ] = 0;                    // return 0
  possible[ 0b001 ] = 0;                    // return 0

  // no vlan ag, yes ip (the value for yes-vlan, yes-ip can be either)
  possible[ 0b010 ] = no_vlan_ip->ip_p;     // return proto
  possible[ 0b011 ] = no_vlan_ip->ip_p;     // return proto

  // middle value ignored
  possible[ 0b100 ] = 0;                    // return 0
  possible[ 0b110 ] = 0;                    // return 0

  // middle value ignored
  possible[ 0b101 ] = single_vlan_ip->ip_p; // return proto
  possible[ 0b111 ] = single_vlan_ip->ip_p; // return proto

  // compute the index we need
  bool has_vlan = single_vlan_hdr->vlan_tag_ethertype == bswap_16(ETHERTYPE_VLAN_TAG);

  bool no_vlan_is_ip     = no_vlan_hdr->ethertype == bswap_16(ETHERTYPE_IP);
  bool single_vlan_is_ip = single_vlan_hdr->ethertype == bswap_16(ETHERTYPE_IP);

  int selector = (int)has_vlan<<2 | (int)no_vlan_is_ip<<1 | (int)single_vlan_is_ip;
  return possible[selector];
}

uint8_t process_packet_branch_free(char const* pkt, size_t sz)
{
  if (sz < sizeof(eth_no_vlan_t) + sizeof(ip_hdr_t)) return 0;
  if (sz >= proc4_req_sz) return proc4(pkt, sz); // likely

  // if we didn't have sufficient size, we must not have a vlan tag
  // (also the packet can't really be an ip packet, because the payload is
  // larger than the size of the vlan tag, but including this for completeness)

  eth_no_vlan_t eth[1];
  ip_hdr_t      ip[1];

  memcpy(eth, pkt,                         sizeof(eth_no_vlan_t));
  memcpy(ip,  pkt + sizeof(eth_no_vlan_t), sizeof(ip_hdr_t));

  uint8_t possible[] = {0, ip->ip_p};
  return possible[ eth->ethertype == ETHERTYPE_IP ];
}
