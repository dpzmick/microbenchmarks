#pragma once

#include <stdint.h>
#include <stddef.h>

// these are probably wrong
enum {
  ETHERTYPE_VLAN_TAG = 0x8100,
  ETHERTYPE_IP       = 0x0800,
  ETHERTYPE_ARP      = 0x0806,
  ETHERTYPE_PTP      = 0x88F7,
};

typedef struct __attribute__((packed)) {
    uint8_t dst[6];
    uint8_t src[6];
} eth_addrs_t;

typedef struct __attribute__((packed)) {
  eth_addrs_t addrs;
  uint16_t    ethertype;
} eth_no_vlan_t;

typedef struct __attribute__((packed)) {
  eth_addrs_t addrs;
  uint16_t    vlan_tag_ethertype; // will be ETHERTYPE_VLAN_TAG
  uint16_t    vlan;               // some vlan id
  uint16_t    ethertype;          // actual ethertype
} eth_single_vlan_t;

typedef struct {
  uint8_t  ip_hl : 4;
  uint8_t  ip_v  : 4;
  uint8_t  ip_tos;
  short    ip_len;
	uint16_t ip_id;
	short    ip_off;
	uint8_t  ip_ttl;
	uint8_t  ip_p;
	uint16_t ip_sum;
	uint32_t srcaddr;
  uint32_t dstaddr;
} ip_hdr_t;

// return ip proto if this is an IP packet
// else 0
// supports packets with 0 and 1 vlan tags
uint8_t process_packet_branch_free(char const* pkt, size_t sz);
uint8_t process_packet_branching(char const* pkt, size_t sz);

// FIXME should be getting the sizes from the packet?
