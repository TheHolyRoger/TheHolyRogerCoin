// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018-2019 The UNIGRID organization
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CSPORKDB_H
#define CSPORKDB_H

#include <boost/filesystem/path.hpp>
#include <dbwrapper.h>
#include <spork.h>

#define MIN_SPORKDB_CACHE_SIZE 1 /* MiB */

class CSporkDB : public CDBWrapper
{
public:
    CSporkDB(size_t nCacheSize = MIN_SPORKDB_CACHE_SIZE << 20, bool fMemory = false, bool fWipe = false);

private:
    CSporkDB(const CSporkDB&);
    void operator=(const CSporkDB&);

public:
    bool WriteSpork(const int nSporkId, const CSporkMessage& spork);
    bool ReadSpork(const int nSporkId, CSporkMessage& spork);
    bool SporkExists(const int nSporkId);
};

extern CSporkDB* pSporkDB;

#endif // CSPORKDB_H
