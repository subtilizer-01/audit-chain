#ifndef LOGCHAIN_H
#define LOGCHAIN_H
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <fstream>
#include "SHA256.h"
using namespace std;
class LogEntry {
    int seq;
    string actor,
        action,
        timestamp,
        detail,
        prevHash,
        entryHash;
public:
    LogEntry(int s, string a, string act, string t, string d, string p)
        : seq(s), actor(a), action(act), timestamp(t), detail(d), prevHash(p){
        entryHash = computeHash();
    }

    LogEntry(int s, string a, string act, string t, string d, string p, string e)
        : seq(s), actor(a), action(act), timestamp(t), detail(d), prevHash(p), entryHash(e) {
    }

    void print() const
    {
        cout << "sequence: " << seq
             << "\naction: " << action
             << "\nactor: " << actor
             << "\ntimestamp: " << timestamp
             << "\ndetail: " << detail
             << "\nprevious entry's Hash: " << prevHash
             << "\ncurrent entry's Hash: " << entryHash << endl;
    }

    int getSeq() const { return seq; }
    string getAction() const { return action; }
    string getActor() const { return actor; }
    string getTimestamp() const { return timestamp; }
    string getDetail() const { return detail; }
    string getPrevHash() const { return prevHash; }
    string getEntryHash() const { return entryHash; }

    string serialize() const
    {
        return to_string(seq) + "|" +
               to_string(actor.size()) + ":" + actor + "|" +
               to_string(action.size()) + ":" + action + "|" +
               to_string(timestamp.size()) + ":" + timestamp + "|" +
               to_string(detail.size()) + ":" + detail + "|" +
               to_string(prevHash.size()) + ":" + prevHash;
    }

    string computeHash() const
    {
        SHA256 sha;                                    //  create the hasher
        sha.update(serialize());                       //  feed it our serialized string
        array<uint8_t, 32> digest = sha.digest();      // finalize -> 32 raw bytes
        return SHA256::toString(digest);               //  convert to 64-char hex, return it
    }
};

class LogChain
{
private:

    static string field(string value)
    {
        return to_string(value.size()) + ":" + value;
    }

    // Reads one "len:value" field starting at pos, advances pos past it.
    static string readField(const string& line, int& pos)
    {
        int colon = line.find(':', pos);
        int len = stoi(line.substr(pos, colon - pos));
        string value = line.substr(colon + 1, len);
        pos = colon + 1 + len + 1;   // skip value and the '|' after it
        return value;
    }

    vector<LogEntry> entries;
public:
    void append(string actor, string action, string timestamp, string detail)
    {
        int seq = entries.size() + 1;
        string prevHash;
        if (entries.empty())
        {
            prevHash = string(64, '0');
        }
        else
        {
            prevHash = entries.back().getEntryHash();   // last entry's hash
        }

        LogEntry a(seq, actor, action, timestamp, detail, prevHash);
        entries.push_back(a);
    }

    void printAll() const
    {
        for (int i = 0; i < entries.size(); i++) {
            entries[i].print();
            cout << "---" << endl;
        }
    }

    bool verify() const
    {
        for (int i = 0; i < entries.size(); i++)
        {
            // CHECK 1; has this entry's own content been altered?
            if (entries[i].computeHash() != entries[i].getEntryHash())
            {
                cout << "TAMPERED: entry " << entries[i].getSeq()
                << " content was modified" << endl;
                return false;
            }

            // CHECK 2; is the link to the previous entry intact?
            if (i == 0)
            {
                if (entries[i].getPrevHash() != string(64, '0'))
                {
                    cout << "TAMPERED: entry " << entries[i].getSeq()
                    << " is not linked to genesis - the original first entry was removed" << endl;
                    return false;
                }
            }
            else
            {
                if (entries[i].getPrevHash() != entries[i - 1].getEntryHash())
                {
                    cout << "TAMPERED: broken link between entry " << entries[i - 1].getSeq()
                    << " and entry " << entries[i].getSeq() << endl;
                    return false;
                }
            }
        }
        return true;
    }

    // Writes every entry to a text file, one entry per line.
    // Each field is length-prefixed (len:value) and separated by '|', so a '|'
    // inside a field's text can never confuse the parser on the way back in.
    bool save(string filename) const
    {
        ofstream out(filename);
        if (!out) {
            cout << "ERROR: could not open " << filename << " for writing" << endl;
            return false;
        }

        for (int i = 0; i < entries.size(); i++)
        {
            out << field(to_string(entries[i].getSeq())) << "|"
                << field(entries[i].getActor()) << "|"
                << field(entries[i].getAction()) << "|"
                << field(entries[i].getTimestamp()) << "|"
                << field(entries[i].getDetail()) << "|"
                << field(entries[i].getPrevHash()) << "|"
                << field(entries[i].getEntryHash()) << endl;
        }

        out.close();
        return true;
    }

    // Reads a log file back into this chain, replacing whatever it held.
    // Nothing is recomputed here. the stored entryHash is loaded as-is, so
    // verify() can compare it against a freshly computed one and catch edits.
    bool load(string filename)
    {
        ifstream in(filename);
        if (!in) {
            cout << "ERROR: could not open " << filename << " for reading" << endl;
            return false;
        }

        entries.clear();
        string line;

        while (getline(in, line))
        {
            if (line.empty()) continue;

            int pos = 0;
            string seqStr = readField(line, pos);
            string actor = readField(line, pos);
            string action = readField(line, pos);
            string timestamp = readField(line, pos);
            string detail = readField(line, pos);
            string prevHash = readField(line, pos);
            string entryHash = readField(line, pos);

            entries.push_back(
                LogEntry(stoi(seqStr), actor, action, timestamp, detail, prevHash, entryHash)
                );
        }

        in.close();
        return true;
    }

    // Writes a checkpoint: the sequence number of the newest entry and its hash.
    // Kept in a SEPARATE file so an attacker who rewrites the log still can't
    // change what the chain looked like at this point in time.
    // In real deployments this file lives somewhere the log process can't
    // write to - a different server, read-only media, or a printed copy.
    bool saveAnchor(string anchorFile) const
    {
        if (entries.empty()) {
            cout << "ERROR: nothing to anchor - the chain is empty" << endl;
            return false;
        }

        ofstream out(anchorFile);
        if (!out) {
            cout << "ERROR: could not open " << anchorFile << " for writing" << endl;
            return false;
        }

        out << entries.back().getSeq() << endl;
        out << entries.back().getEntryHash() << endl;
        out.close();

        cout << "Anchored entry " << entries.back().getSeq() << endl;
        return true;
    }

    // Compares the chain against the checkpoint. The chain is ALLOWED to have
    // grown since - what must not change is the entry that was anchored.
    // If entry N's hash still matches, everything up to N is untouched, because
    // each hash depends on every entry before it.
    bool verifyAgainstAnchor(string anchorFile) const
    {
        ifstream in(anchorFile);
        if (!in)
        {
            cout << "ERROR: could not open " << anchorFile << " for reading" << endl;
            return false;
        }

        int anchoredSeq;
        string anchoredHash;
        in >> anchoredSeq >> anchoredHash;
        in.close();

        // The chain is shorter than the checkpoint - entries were deleted.
        if (anchoredSeq > (int)entries.size())
        {
            cout << "TAMPERED: entry " << anchoredSeq
                 << " was anchored but the chain now ends at entry " << entries.size()
                 << " - history was truncated" << endl;
            return false;
        }

        // entries[i] holds seq i+1, so the anchored entry sits at index seq-1.
        const LogEntry& anchored = entries[anchoredSeq - 1];

        if (anchored.getEntryHash() != anchoredHash)
        {
            cout << "TAMPERED: entry " << anchoredSeq
                 << " no longer matches the anchor - history was rewritten" << endl;
            return false;
        }

        cout << "Anchor check passed: entry " << anchoredSeq << " is unchanged";
        if ((int)entries.size() > anchoredSeq)
            cout << " (" << entries.size() - anchoredSeq << " entries appended since)";
        cout << endl;
        return true;
    }

    // ─── ATTACK SIMULATION ONLY - NOT A REAL FEATURE ───
    // Models a sophisticated attacker who has the source code: after editing an
    // entry's content, they rebuild every hash downstream so the chain becomes
    // internally consistent again and verify() passes clean.
    // This is exactly the attack that plain hash chaining CANNOT detect - and
    // exactly what the external anchor exists to catch.
    void simulateFullRewrite()
    {
        for (int i = 0; i < entries.size(); i++)
        {
            string prevHash;
            if (i == 0)
                prevHash = string(64, '0');
            else
                prevHash = entries[i - 1].getEntryHash();

            // Rebuild the entry so its hash is recomputed from its (edited) contents
            entries[i] = LogEntry(
                entries[i].getSeq(),
                entries[i].getActor(),
                entries[i].getAction(),
                entries[i].getTimestamp(),
                entries[i].getDetail(),
                prevHash
                );
        }
        cout << "[ATTACK] All hashes recomputed - chain is now internally consistent." << endl;
    }

    int size() const { return entries.size(); }
    const LogEntry& at(int i) const { return entries[i]; }

    // ─── Merkle tree ──────────────────────────────────────────────────────
    // The hash chain proves the whole log is intact, but to prove ONE entry
    // belongs you would have to hand over every record. A Merkle tree fixes
    // that: entry hashes are paired and hashed upward until a single root
    // remains, so membership can be proven with ~log2(n) hashes instead of n.
    // Same structure used by certificate transparency logs and git.

    static string hashPair(const string& left, const string& right)
    {
        SHA256 sha;
        sha.update(left + right);
        return SHA256::toString(sha.digest());
    }

    // Bottom level of the tree: one leaf per entry.
    vector<string> merkleLeaves() const
    {
        vector<string> leaves;
        for (int i = 0; i < (int)entries.size(); i++)
            leaves.push_back(entries[i].getEntryHash());
        return leaves;
    }

    // Collapses the leaves upward, pairing as it goes. An odd node at any
    // level is paired with itself, which keeps the tree balanced without
    // inventing data.
    string merkleRoot() const
    {
        vector<string> level = merkleLeaves();
        if (level.empty()) return string(64, '0');

        while (level.size() > 1) {
            vector<string> next;
            for (size_t i = 0; i < level.size(); i += 2) {
                const string& left  = level[i];
                const string& right = (i + 1 < level.size()) ? level[i + 1] : level[i];
                next.push_back(hashPair(left, right));
            }
            level = next;
        }
        return level[0];
    }

    // One step of a proof: the sibling hash, and which side it sits on.
    struct ProofStep {
        string hash;
        bool siblingIsRight;
    };

    // The sibling hashes needed to rebuild the root from one leaf. Everything
    // else in the tree stays private - this is what makes the proof compact.
    vector<ProofStep> merkleProof(int index) const
    {
        vector<ProofStep> proof;
        if (index < 0 || index >= (int)entries.size()) return proof;

        vector<string> level = merkleLeaves();
        size_t pos = index;

        while (level.size() > 1) {
            const bool isLeft = (pos % 2 == 0);
            const size_t siblingPos = isLeft ? pos + 1 : pos - 1;

            ProofStep step;
            step.siblingIsRight = isLeft;
            step.hash = (siblingPos < level.size()) ? level[siblingPos] : level[pos];
            proof.push_back(step);

            vector<string> next;
            for (size_t i = 0; i < level.size(); i += 2) {
                const string& left  = level[i];
                const string& right = (i + 1 < level.size()) ? level[i + 1] : level[i];
                next.push_back(hashPair(left, right));
            }
            level = next;
            pos /= 2;
        }
        return proof;
    }

    // Rebuilds the root from a leaf plus its proof path. If it matches the
    // expected root, that leaf is provably part of the tree - without the
    // verifier ever seeing the other entries.
    static bool verifyMerkleProof(const string& leafHash,
                                  const vector<ProofStep>& proof,
                                  const string& expectedRoot)
    {
        string running = leafHash;
        for (size_t i = 0; i < proof.size(); i++) {
            running = proof[i].siblingIsRight
                          ? hashPair(running, proof[i].hash)
                          : hashPair(proof[i].hash, running);
        }
        return running == expectedRoot;
    }

    // Anchoring the ROOT is strictly stronger than anchoring the head hash:
    // the root commits to every entry, so altering any one of them changes it.
    bool saveMerkleAnchor(string anchorFile) const
    {
        if (entries.empty()) return false;
        ofstream out(anchorFile);
        if (!out) return false;
        out << entries.size() << "\n" << merkleRoot() << "\n";
        return true;
    }

    bool verifyAgainstMerkleAnchor(string anchorFile, string& detailOut) const
    {
        ifstream in(anchorFile);
        if (!in) { detailOut = "no merkle anchor found"; return false; }

        int anchoredCount;
        string anchoredRoot;
        in >> anchoredCount >> anchoredRoot;

        if (anchoredCount != (int)entries.size()) {
            detailOut = "entry count changed since anchoring ("
                        + to_string(anchoredCount) + " -> " + to_string(entries.size()) + ")";
            return false;
        }
        if (merkleRoot() != anchoredRoot) {
            detailOut = "merkle root does not match the anchored root";
            return false;
        }
        detailOut = "merkle root matches";
        return true;
    }
};

#endif // LOGCHAIN_H
