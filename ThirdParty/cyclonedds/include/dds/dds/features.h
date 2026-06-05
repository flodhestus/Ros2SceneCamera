// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _DDS_PUBLIC_FEATURES_H_
#define _DDS_PUBLIC_FEATURES_H_

/* Plugin CycloneDDS build: disabled features (must match ddsc.lib build). */
#undef DDS_HAS_SECURITY
#undef DDS_HAS_LIFESPAN
#undef DDS_HAS_TCP_TLS
#undef DDS_ALLOW_NESTED_DOMAIN
#undef DDS_IS_STATIC_LIBRARY
#undef DDS_HAS_QOS_PROVIDER

#define DDS_HAS_DEADLINE_MISSED 1
#define DDS_HAS_NETWORK_PARTITIONS 1
#define DDS_HAS_TYPELIB 1
#define DDS_HAS_TYPE_DISCOVERY 1
#define DDS_HAS_TOPIC_DISCOVERY 1
#define DDS_HAS_TCP 1

#endif