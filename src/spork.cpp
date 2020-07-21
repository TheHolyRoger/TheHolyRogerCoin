// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2018-2019 The UNIGRID organization
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <spork.h>
#include <base58.h>
#include <key.h>
#include <net.h>
#include <netmessagemaker.h>
#include <net_processing.h>
#include <protocol.h>
#include <sync.h>
#include <signer.h>
#include <sporkdb.h>
#include <sporknames.h>
#include <util.h>
#include <validation.h>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;
std::map<int, CSporkMessage> mapSporksActive;

void LoadSporksFromDB()
{
    for (int i = SPORK_START; i <= SPORK_END; ++i)
    {
        // Since not all spork IDs are in use, we have to exclude undefined IDs
        std::string strSpork = sporkManager.GetSporkNameByID(i);
        if (strSpork == "Unknown") continue;

        // attempt to read spork from sporkDB
        CSporkMessage spork;

        if (!pSporkDB->ReadSpork(i, spork)) {
            LogPrintf("%s : no previous value for %s found in database\n", __func__, strSpork);
            continue;
        }

        // add spork to memory
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        std::time_t result = spork.nValue;

        // If SPORK Value is greater than 1,000,000 assume it's actually a Date and then convert to a more readable format
        if (spork.nValue > 1000000) {
            LogPrintf("%s : loaded spork %s with value %d : %s", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue,
                      std::ctime(&result));
        } else {
            LogPrintf("%s : loaded spork %s with value %d\n", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue);
        }
    }
}

void ProcessSpork(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (pfrom->nVersion < SPORK_VERSION)
        return;

    if (strCommand == NetMsgType::SPORK) {
        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == nullptr) return;

        // Ignore spork messages about unknown/deleted sporks
        std::string strSpork = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (strSpork == "Unknown") return;

        uint256 hash = spork.GetHash();

        if (mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                LogPrintf("%s : seen %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
                return;
            } else {
                LogPrintf("%s : got updated spork %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
            }
        }

        if (!sporkManager.CheckSignature(spork, true)) {
            LogPrintf("%s : Invalid Signature\n", __func__);
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);
        pSporkDB->WriteSpork(spork.nSporkID, spork);
    }

    if (strCommand == NetMsgType::GETSPORKS) {
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();
        CNetMsgMaker msgMaker(pfrom->GetSendVersion());

        while (it != mapSporksActive.end()) {
            g_connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SPORK, it->second));
            it++;
        }
    }
}

int64_t GetSporkValue(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_BLACKLIST_BLOCK_REFERENCE) r = SPORK_1_BLACKLIST_BLOCK_REFERENCE_DEFAULT;
        if (r == -1) LogPrintf("%s : Unknown Spork %d\n", __func__, nSporkID);
    }

    return r;
}

// Grab the spork value, and see if it's off
bool IsSporkActive(int nSporkID)
{
    int64_t r = GetSporkValue(nSporkID);

    if (r == -1)
        return false;

    if (nSporkID == SPORK_1_BLACKLIST_BLOCK_REFERENCE) {
        if (((r >> 16) & 0xffffffffffff) > 0)
            return true;

        return false;
    }

    return r < GetTime();
}

bool CSporkManager::CheckSignature(CSporkMessage& spork, bool fCheckSigner)
{
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) +
                             boost::lexical_cast<std::string>(spork.nTimeSigned);
    CPubKey pubkey(ParseHex(Params().SporkKey()));
    std::string errorMessage = "";

    bool valid = sporkSigner.VerifyMessage(pubkey, spork.vchSig, strMessage, errorMessage);

    if (fCheckSigner && !valid) {
        LogPrintf("%s : %s\n", __func__, errorMessage.c_str());
        return false;
    }

    return true;
}

bool CSporkManager::Sign(CSporkMessage& spork)
{
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) +
                             boost::lexical_cast<std::string>(spork.nTimeSigned);
    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!sporkSigner.SetKey(privKey, errorMessage, key2, pubkey2)) {
        LogPrintf("%s [ERROR] : Invalid spork key: '%s'\n", __func__, errorMessage);
        return false;
    }

    if (!sporkSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrintf("%s : Sign message failed\n", __func__);
        return false;
    }

    if (!sporkSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrintf("%s : Verify message failed\n", __func__);
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{
    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg)
{
    CInv inv(MSG_SPORK, msg.GetHash());
    RelayInv(inv, g_connman.get());
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage msg;
    privKey = strPrivKey;
    Sign(msg);

    if (CheckSignature(msg, true)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_1_BLACKLIST_BLOCK_REFERENCE") return SPORK_1_BLACKLIST_BLOCK_REFERENCE;
    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    if (id == SPORK_1_BLACKLIST_BLOCK_REFERENCE) return "SPORK_1_BLACKLIST_BLOCK_REFERENCE";
    return "Unknown";
}
