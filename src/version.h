// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

static const int PROTOCOL_VERSION = 70020;

static const int INIT_PROTO_VERSION = 209;

static const int GETHEADERS_VERSION = 70002;
static const int MIN_PEER_PROTO_VERSION = GETHEADERS_VERSION;
static const int CADDR_TIME_VERSION = 31402;
static const int BIP0031_VERSION = 60000;
static const int NO_BLOOM_VERSION = 70011;
static const int SENDHEADERS_VERSION = 70012;
static const int FEEFILTER_VERSION = 70013;
static const int SHORT_IDS_BLOCKS_VERSION = 70014;
static const int INVALID_CB_NO_BAN_VERSION = 70015;
static const int SPORK_VERSION = 70020;

#endif // BITCOIN_VERSION_H
