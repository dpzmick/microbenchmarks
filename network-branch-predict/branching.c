#include "pkt.h"
#include "assume.h"

#include <byteswap.h>
#include <stdbool.h>
#include <string.h>

static inline uint8_t proc_ip(char const * pkt, size_t sz)
{
  if (sz < sizeof(ip_hdr_t)) return 0;

  ip_hdr_t ip[1];
  memcpy(ip, pkt, sizeof(ip_hdr_t));

  return ip->ip_p; // already right order, single byte
}

uint8_t process_packet_branching(char const* pkt, size_t sz)
{
  if (sz < sizeof(eth_no_vlan_t)) return 0;
  eth_no_vlan_t no_vlan_hdr[1];
  memcpy(no_vlan_hdr, pkt, sizeof(eth_no_vlan_t));

  if (no_vlan_hdr->ethertype == bswap_16(ETHERTYPE_VLAN_TAG)) {
    if (sz < sizeof(eth_single_vlan_t)) return 0;

    eth_single_vlan_t single_vlan_hdr[1];
    memcpy(single_vlan_hdr, pkt, sizeof(single_vlan_hdr));

    if (single_vlan_hdr->ethertype != bswap_16(ETHERTYPE_IP)) return 0;
    char const* ip_pkt = pkt + sizeof(eth_single_vlan_t);
    size_t      ip_sz  = sz - sizeof(eth_single_vlan_t); // already checked
    return proc_ip(ip_pkt, ip_sz);
  }
  else {
    // there's no vlan tag, so we already have the pkt
    if (no_vlan_hdr->ethertype != bswap_16(ETHERTYPE_IP)) return 0;
    char const* ip_pkt = pkt + sizeof(eth_no_vlan_t);
    size_t      ip_sz  = sz - sizeof(eth_no_vlan_t); // already checked
    return proc_ip(ip_pkt, ip_sz);
  }

  __builtin_unreachable();
}
