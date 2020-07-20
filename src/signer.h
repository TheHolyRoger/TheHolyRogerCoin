// Copyright (c) 2014-2015 The Darkcoin developers
// Copyright (c) 2015-2020 The Neutron Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIGNER_H
#define SIGNER_H

#include <map>
#include <string>
#include <vector>
#include <uint256.h>
#include <net.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <key.h>

class CSporkSigner;

extern CSporkSigner sporkSigner;
extern std::string strMasterNodePrivKey;

class CSporkSigner
{
public:
    bool SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey);
    bool SignMessage(std::string strMessage, std::string& errorMessage, std::vector<unsigned char>& vchSig, CKey key);
    bool VerifyMessage(CPubKey pubkey, std::vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage);
};

#endif
