/*
 * DPVS is a software load balancer (Virtual Server) based on DPDK.
 *
 * Copyright (C) 2021 iQIYI (www.iqiyi.com).
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "ipset/ipset_hash.h"
#include "ipset/pfxlen.h"

typedef struct hash_ipport_elem4 {
    uint32_t ip;
    uint16_t port;
    uint8_t proto;

    char comment[IPSET_MAXCOMLEN];
} elem4_t;

static int
hash_ipport_data_equal4(const void *elem1, const void *elem2)
{
    elem4_t *e1 = (elem4_t *)elem1;
    elem4_t *e2 = (elem4_t *)elem2;

    return e1->ip == e2->ip && e1->port == e2->port && e1->proto == e2->proto;
}

static void
hash_ipport_do_list4(struct ipset_member *member, void *elem, bool comment)
{
    elem4_t *e = (elem4_t *)elem;

    member->port = ntohs(e->port);
    member->proto = e->proto;
    member->addr.in.s_addr = e->ip;
    if (comment)
        rte_strlcpy(member->comment, e->comment, IPSET_MAXCOMLEN);
}

static uint32_t
hash_ipport_hashkey4(void *data, int len, uint32_t mask)
{
    elem4_t *e = (elem4_t *)data;

    return (e->ip * 31 + e->port * 31 + e->proto) & mask;
}

static int
hash_ipport_adt4(int opcode, struct ipset *set, struct ipset_param *param)
{
    elem4_t e;
    int ret;
    uint16_t port;
    uint32_t ip, ip_to;
    ipset_adtfn adtfn = set->type->adtfn[opcode];

    if (set->family != param->option.family)
        return EDPVS_INVAL;

    memset(&e, 0, sizeof(e));

    if (opcode == IPSET_OP_TEST) {
        e.ip = param->range.min_addr.in.s_addr;
        e.proto = param->proto;
        e.port = htons(param->range.min_port);

        return adtfn(set, &e, 0);
    }

    if (set->comment && opcode == IPSET_OP_ADD)
        rte_strlcpy(e.comment, param->comment, IPSET_MAXCOMLEN);

    ip = ntohl(param->range.min_addr.in.s_addr);
    if (param->cidr) {
        ip_set_mask_from_to(ip, ip_to, param->cidr);
    } else {
        ip_to = ntohl(param->range.max_addr.in.s_addr);
    }
    for (; ip <= ip_to; ip++) {
        e.ip = htonl(ip);
        e.proto = param->proto;
        for (port = param->range.min_port; port >= param->range.min_port &&
                port <= param->range.max_port; port++) {
            e.port = htons(port);
            ret = adtfn(set, &e, param->flag);
            if (ret)
                return ret;
        }
    }
    return EDPVS_OK;
}

static int
hash_ipport_test4(struct ipset *set, struct ipset_test_param *p)
{
    elem4_t e;
    uint16_t *ports, _ports[2];
    struct dp_vs_iphdr *iph = p->iph;

    memset(&e, 0, sizeof(e));

    ports = mbuf_header_pointer(p->mbuf, iph->len, sizeof(_ports), _ports);

    e.proto = iph->proto;
    if (p->direction == 1) {
        e.ip = iph->saddr.in.s_addr;
        e.port = ports[0];
    } else {
        e.ip = iph->daddr.in.s_addr;
        e.port = ports[1];
    }

    return set->type->adtfn[IPSET_OP_TEST](set, &e, 0);
}

struct ipset_type_variant hash_ipport_variant4 = {
    .adt = hash_ipport_adt4,
    .test = hash_ipport_test4,
    .hash.do_compare = hash_ipport_data_equal4,
    .hash.do_list = hash_ipport_do_list4,
    .hash.do_hash = hash_ipport_hashkey4
};

typedef struct hash_ipport_elem6 {
    struct in6_addr ip;
    uint16_t port;
    uint8_t proto;

    char comment[IPSET_MAXCOMLEN];
} elem6_t;

static int
hash_ipport_data_equal6(const void *elem1, const void *elem2)
{
    elem6_t *e1 = (elem6_t *)elem1;
    elem6_t *e2 = (elem6_t *)elem2;

    return !memcmp(e1->ip.s6_addr, e2->ip.s6_addr, 16) &&
           e1->port == e2->port && e1->proto == e2->proto;
}

static void
hash_ipport_do_list6(struct ipset_member *member, void *elem, bool comment)
{
    elem6_t *e = (elem6_t *)elem;

    member->port = ntohs(e->port);
    member->proto = e->proto;
    member->addr.in6 = e->ip;
    if (comment)
        rte_strlcpy(member->comment, e->comment, IPSET_MAXCOMLEN);
}

static int
hash_ipport_adt6(int opcode, struct ipset *set, struct ipset_param *param)
{
    int ret;
    uint16_t port;
    elem6_t e;
    ipset_adtfn adtfn = set->type->adtfn[opcode];

    if (set->family != param->option.family)
        return EDPVS_INVAL;

    memset(&e, 0, sizeof(e));

    e.ip = param->range.min_addr.in6;
    e.proto = param->proto;

    if (opcode == IPSET_OP_TEST) {
        e.port = htons(param->range.min_port);
        return adtfn(set, &e, 0);
    }

    if (set->comment && opcode == IPSET_OP_ADD)
        rte_strlcpy(e.comment, param->comment, IPSET_MAXCOMLEN);

    for (port = param->range.min_port; port >= param->range.min_port &&
            port <= param->range.max_port; port++) {
        e.port = htons(port);
        ret = adtfn(set, &e, param->flag);
        if (ret)
            return ret;
    }

    return EDPVS_OK;
}

static int
hash_ipport_test6(struct ipset *set, struct ipset_test_param *p)
{
    elem6_t e;
    uint16_t *ports, _ports[2];
    struct dp_vs_iphdr *iph = p->iph;

    memset(&e, 0, sizeof(e));

    ports = mbuf_header_pointer(p->mbuf, iph->len, sizeof(_ports), _ports);

    e.proto = iph->proto;
    if (p->direction == 1) {
        e.ip = iph->saddr.in6;
        e.port = ports[0];
    } else {
        e.ip = iph->daddr.in6;
        e.port = ports[1];
    }

    return set->type->adtfn[IPSET_OP_TEST](set, &e, 0);
}

struct ipset_type_variant hash_ipport_variant6 = {
    .adt = hash_ipport_adt6,
    .test = hash_ipport_test6,
    .hash.do_compare = hash_ipport_data_equal6,
    .hash.do_list = hash_ipport_do_list6,
    .hash.do_hash = jhash_hashkey
};

static int
hash_ipport_create(struct ipset *set, struct ipset_param *param)
{
    hash_create(set, param);

    if (param->option.family == AF_INET) {
        set->dsize = sizeof(elem4_t);
        set->hash_len = offsetof(elem4_t, comment);
        set->variant = &hash_ipport_variant4;
    } else {
        set->dsize = sizeof(elem6_t);
        set->hash_len = offsetof(elem6_t, comment);
        set->variant = &hash_ipport_variant6;
    }

    return EDPVS_OK;
}

struct ipset_type hash_ipport_type = {
    .name       = "hash:ip,port",
    .create     = hash_ipport_create,
    .destroy    = hash_destroy,
    .flush      = hash_flush,
    .list       = hash_list,
    .adtfn      = hash_adtfn,
};
