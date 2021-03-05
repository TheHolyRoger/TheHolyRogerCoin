// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2018-2019 The UNIGRID organization
// Copyright (c) 2020-2021 The Holy Roger developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORK_H
#define SPORK_H

#include <base58.h>
#include <key.h>
#include <net.h>
#include <sync.h>
#include <util.h>
#include <utilstrencodings.h>

#include <protocol.h>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

// Never reuse sporks unless older ones are banned from the network
#define SPORK_START 10001
#define SPORK_END 10002

#define SPORK_1_BLACKLIST_BLOCK_REFERENCE_DEFAULT 0              // OFF
#define SPORK_2_ALLOWED_ALGO_MASK_DEFAULT         0b0001         // [RESERVER] [RESERVED] [ARSE] [SCRYPT]

class CSporkMessage;
class CSporkManager;

extern std::map<uint256, CSporkMessage> mapSporks;
extern std::map<int, CSporkMessage> mapSporksActive;
extern CSporkManager sporkManager;

void LoadSporksFromDB();
void ProcessSpork(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv);
int64_t GetSporkValue(int nSporkID);
bool IsSporkActive(int nSporkID);

class CSporkMessage
{
public:
    std::vector<unsigned char> vchSig;
    int nSporkID;
    int64_t nValue;
    int64_t nTimeSigned;

    uint256 GetHash()
    {
        uint256 n = Hash(BEGIN(nSporkID), END(nTimeSigned));
        return n;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nSporkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
    }
};

class CSporkManager
{
private:
    std::vector<unsigned char> vchSig;
    std::string privKey;

public:
    CSporkManager()
    {
    }

    std::string GetSporkNameByID(int id);
    int GetSporkIDByName(std::string strName);
    bool UpdateSpork(int nSporkID, int64_t nValue);
    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CSporkMessage& spork, bool fCheckSigner = false);
    bool Sign(CSporkMessage& spork);
    void Relay(CSporkMessage& msg);
};

#endif
