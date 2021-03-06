// -*- mode: C++; c-file-style: "cc-mode" -*-
//=============================================================================
//
// THIS MODULE IS PUBLICLY LICENSED
//
// Copyright 2001-2020 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//=============================================================================
///
/// \file
/// \brief C++ Tracing in VCD Format
///
//=============================================================================
// SPDIFF_OFF

#ifndef _VERILATED_VCD_C_H_
#define _VERILATED_VCD_C_H_ 1

#include "verilatedos.h"
#include "verilated.h"

#include <map>
#include <string>
#include <vector>

class VerilatedVcd;
class VerilatedVcdCallInfo;

// SPDIFF_ON
//=============================================================================
// VerilatedFile
/// File handling routines, which can be overrode for e.g. socket I/O

class VerilatedVcdFile {
private:
    int m_fd;  ///< File descriptor we're writing to
public:
    // METHODS
    VerilatedVcdFile()
        : m_fd(0) {}
    virtual ~VerilatedVcdFile() {}
    virtual bool open(const std::string& name) VL_MT_UNSAFE;
    virtual void close() VL_MT_UNSAFE;
    virtual ssize_t write(const char* bufp, ssize_t len) VL_MT_UNSAFE;
};

//=============================================================================
// VerilatedVcdSig
/// Internal data on one signal being traced.

class VerilatedVcdSig {
protected:
    friend class VerilatedVcd;
    vluint32_t m_code;  ///< VCD file code number
    int m_bits;  ///< Size of value in bits
    VerilatedVcdSig(vluint32_t code, int bits)
        : m_code(code)
        , m_bits(bits) {}

public:
    ~VerilatedVcdSig() {}
};

//=============================================================================

typedef void (*VerilatedVcdCallback_t)(VerilatedVcd* vcdp, void* userthis, vluint32_t code);

//=============================================================================
// VerilatedVcd
/// Base class to create a Verilator VCD dump
/// This is an internally used class - see VerilatedVcdC for what to call from applications

class VerilatedVcd {
private:
    VerilatedVcdFile* m_filep;  ///< File we're writing to
    bool m_fileNewed;  ///< m_filep needs destruction
    bool m_isOpen;  ///< True indicates open file
    bool m_evcd;  ///< True for evcd format
    std::string m_filename;  ///< Filename we're writing to (if open)
    vluint64_t m_rolloverMB;  ///< MB of file size to rollover at
    char m_scopeEscape;  ///< Character to separate scope components
    int m_modDepth;  ///< Depth of module hierarchy
    bool m_fullDump;  ///< True indicates dump ignoring if changed
    vluint32_t m_nextCode;  ///< Next code number to assign
    std::string m_modName;  ///< Module name being traced now
    double m_timeRes;  ///< Time resolution (ns/ms etc)
    double m_timeUnit;  ///< Time units (ns/ms etc)
    vluint64_t m_timeLastDump;  ///< Last time we did a dump

    char* m_wrBufp;  ///< Output buffer
    char* m_wrFlushp;  ///< Output buffer flush trigger location
    char* m_writep;  ///< Write pointer into output buffer
    vluint64_t m_wrChunkSize;  ///< Output buffer size
    vluint64_t m_wroteBytes;  ///< Number of bytes written to this file

    std::vector<char> m_suffixes;  ///< VCD line end string codes + metadata
    const char* m_suffixesp;  ///< Pointer to first element of above

    vluint32_t* m_sigs_oldvalp;  ///< Pointer to old signal values
    typedef std::vector<VerilatedVcdSig> SigVec;
    SigVec m_sigs;  ///< Pointer to signal information
    typedef std::vector<VerilatedVcdCallInfo*> CallbackVec;
    CallbackVec m_callbacks;  ///< Routines to perform dumping
    typedef std::map<std::string, std::string> NameMap;
    NameMap* m_namemapp;  ///< List of names for the header

    VerilatedAssertOneThread m_assertOne;  ///< Assert only called from single thread

    void bufferResize(vluint64_t minsize);
    void bufferFlush() VL_MT_UNSAFE_ONE;
    inline void bufferCheck() {
        // Flush the write buffer if there's not enough space left for new information
        // We only call this once per vector, so we need enough slop for a very wide "b###" line
        if (VL_UNLIKELY(m_writep > m_wrFlushp)) { bufferFlush(); }
    }
    void closePrev();
    void closeErr();
    void openNext();
    void makeNameMap();
    void deleteNameMap();
    void printIndent(int level_change);
    void printStr(const char* str);
    void printQuad(vluint64_t n);
    void printTime(vluint64_t timeui);
    void declare(vluint32_t code, const char* name, const char* wirep, bool array, int arraynum,
                 bool tri, bool bussed, int msb, int lsb);

    void dumpHeader();
    void dumpPrep(vluint64_t timeui);
    void dumpFull(vluint64_t timeui);
    // cppcheck-suppress functionConst
    void dumpDone();
    char* writeCode(char* writep, vluint32_t code);

    void finishLine(vluint32_t* oldp, char* writep);

    // CONSTRUCTORS
    VL_UNCOPYABLE(VerilatedVcd);

public:
    explicit VerilatedVcd(VerilatedVcdFile* filep = NULL);
    ~VerilatedVcd();
    /// Routines can only be called from one thread; allow next call from different thread
    void changeThread() { m_assertOne.changeThread(); }

    // ACCESSORS
    /// Set size in megabytes after which new file should be created
    void rolloverMB(vluint64_t rolloverMB) { m_rolloverMB = rolloverMB; }
    /// Is file open?
    bool isOpen() const { return m_isOpen; }
    /// Change character that splits scopes.  Note whitespace are ALWAYS escapes.
    void scopeEscape(char flag) { m_scopeEscape = flag; }
    /// Is this an escape?
    inline bool isScopeEscape(char c) { return isspace(c) || c == m_scopeEscape; }

    // METHODS
    /// Open the file; call isOpen() to see if errors
    void open(const char* filename) VL_MT_UNSAFE_ONE;
    void openNext(bool incFilename);  ///< Open next data-only file
    void close() VL_MT_UNSAFE_ONE;  ///< Close the file
    /// Flush any remaining data to this file
    void flush() VL_MT_UNSAFE_ONE { bufferFlush(); }
    /// Flush any remaining data from all files
    static void flush_all() VL_MT_UNSAFE_ONE;

    void set_time_unit(const char* unitp);  ///< Set time units (s/ms, defaults to ns)
    void set_time_unit(const std::string& unit) { set_time_unit(unit.c_str()); }

    void set_time_resolution(const char* unitp);  ///< Set time resolution (s/ms, defaults to ns)
    void set_time_resolution(const std::string& unit) { set_time_resolution(unit.c_str()); }

    double timescaleToDouble(const char* unitp);
    std::string doubleToTimescale(double value);

    /// Inside dumping routines, called each cycle to make the dump
    void dump(vluint64_t timeui);
    /// Call dump with a absolute unscaled time in seconds
    void dumpSeconds(double secs) { dump(static_cast<vluint64_t>(secs * m_timeRes)); }

    /// Inside dumping routines, declare callbacks for tracings
    void addCallback(VerilatedVcdCallback_t initcb, VerilatedVcdCallback_t fullcb,
                     VerilatedVcdCallback_t changecb, void* userthis) VL_MT_UNSAFE_ONE;

    /// Inside dumping routines, declare a module
    void module(const std::string& name);
    /// Inside dumping routines, declare a signal
    void declBit(vluint32_t code, const char* name, bool array, int arraynum);
    void declBus(vluint32_t code, const char* name, bool array, int arraynum, int msb, int lsb);
    void declQuad(vluint32_t code, const char* name, bool array, int arraynum, int msb, int lsb);
    void declArray(vluint32_t code, const char* name, bool array, int arraynum, int msb, int lsb);
    void declFloat(vluint32_t code, const char* name, bool array, int arraynum);
    void declDouble(vluint32_t code, const char* name, bool array, int arraynum);
#ifndef VL_TRACE_VCD_OLD_API
    void declTriBit(vluint32_t code, const char* name, bool array, int arraynum);
    void declTriBus(vluint32_t code, const char* name, bool array, int arraynum, int msb, int lsb);
    void declTriQuad(vluint32_t code, const char* name, bool array, int arraynum, int msb,
                     int lsb);
    void declTriArray(vluint32_t code, const char* name, bool array, int arraynum, int msb,
                      int lsb);
#endif  // VL_TRACE_VCD_OLD_API
    //  ... other module_start for submodules (based on cell name)

    //=========================================================================
    // Inside dumping routines used by Verilator

    vluint32_t* oldp(vluint32_t code) { return m_sigs_oldvalp + code; }

#ifndef VL_TRACE_VCD_OLD_API

    //=========================================================================
    // Write back to previous value buffer value and emit

    void fullBit(vluint32_t* oldp, vluint32_t newval);
    template <int T_Bits> void fullBus(vluint32_t* oldp, vluint32_t newval);
    void fullQuad(vluint32_t* oldp, vluint64_t newval, int bits);
    void fullArray(vluint32_t* oldp, const vluint32_t* newvalp, int bits);
    void fullFloat(vluint32_t* oldp, float newval);
    void fullDouble(vluint32_t* oldp, double newval);

    //=========================================================================
    // Check previous value and emit if changed

    inline void chgBit(vluint32_t* oldp, vluint32_t newval) {
        const vluint32_t diff = *oldp ^ newval;
        if (VL_UNLIKELY(diff)) fullBit(oldp, newval);
    }
    template <int T_Bits> inline void chgBus(vluint32_t* oldp, vluint32_t newval) {
        const vluint32_t diff = *oldp ^ newval;
        if (VL_UNLIKELY(diff)) fullBus<T_Bits>(oldp, newval);
    }
    inline void chgQuad(vluint32_t* oldp, vluint64_t newval, int bits) {
        const vluint64_t diff = *reinterpret_cast<vluint64_t*>(oldp) ^ newval;
        if (VL_UNLIKELY(diff)) fullQuad(oldp, newval, bits);
    }
    inline void chgArray(vluint32_t* oldp, const vluint32_t* newvalp, int bits) {
        for (int i = 0; i < (bits + 31) / 32; ++i) {
            if (VL_UNLIKELY(oldp[i] ^ newvalp[i])) {
                fullArray(oldp, newvalp, bits);
                return;
            }
        }
    }
    inline void chgFloat(vluint32_t* oldp, float newval) {
        // cppcheck-suppress invalidPointerCast
        if (VL_UNLIKELY(*reinterpret_cast<float*>(oldp) != newval)) fullFloat(oldp, newval);
    }
    inline void chgDouble(vluint32_t* oldp, double newval) {
        // cppcheck-suppress invalidPointerCast
        if (VL_UNLIKELY(*reinterpret_cast<double*>(oldp) != newval)) fullDouble(oldp, newval);
    }

#else  // VL_TRACE_VCD_OLD_API

    // Note: These are only for testing for backward compatibility. Verilator
    // should use the more efficient versions above.

    //=========================================================================
    // Write back to previous value buffer value and emit

    void fullBit(vluint32_t* oldp, vluint32_t newval) { fullBit(oldp - m_sigs_oldvalp, newval); }
    template <int T_Bits> void fullBus(vluint32_t* oldp, vluint32_t newval) {
        fullBus(oldp - m_sigs_oldvalp, newval, T_Bits);
    }
    void fullQuad(vluint32_t* oldp, vluint64_t newval, int bits) {
        fullQuad(oldp - m_sigs_oldvalp, newval, bits);
    }
    void fullArray(vluint32_t* oldp, const vluint32_t* newvalp, int bits) {
        fullArray(oldp - m_sigs_oldvalp, newvalp, bits);
    }
    void fullFloat(vluint32_t* oldp, float newval) { fullFloat(oldp - m_sigs_oldvalp, newval); }
    void fullDouble(vluint32_t* oldp, double newval) { fullDouble(oldp - m_sigs_oldvalp, newval); }

    //=========================================================================
    // Check previous value and emit if changed

    void chgBit(vluint32_t* oldp, vluint32_t newval) { chgBit(oldp - m_sigs_oldvalp, newval); }
    template <int T_Bits> void chgBus(vluint32_t* oldp, vluint32_t newval) {
        chgBus(oldp - m_sigs_oldvalp, newval, T_Bits);
    }
    void chgQuad(vluint32_t* oldp, vluint64_t newval, int bits) {
        chgQuad(oldp - m_sigs_oldvalp, newval, bits);
    }
    void chgArray(vluint32_t* oldp, const vluint32_t* newvalp, int bits) {
        chgArray(oldp - m_sigs_oldvalp, newvalp, bits);
    }
    void chgFloat(vluint32_t* oldp, float newval) { chgFloat(oldp - m_sigs_oldvalp, newval); }
    void chgDouble(vluint32_t* oldp, double newval) { chgDouble(oldp - m_sigs_oldvalp, newval); }

    /// Inside dumping routines, dump one signal, faster when not inlined
    /// due to code size reduction.
    void fullBit(vluint32_t code, const vluint32_t newval);
    void fullBus(vluint32_t code, const vluint32_t newval, int bits);
    void fullQuad(vluint32_t code, const vluint64_t newval, int bits);
    void fullArray(vluint32_t code, const vluint32_t* newvalp, int bits);
    void fullArray(vluint32_t code, const vluint64_t* newvalp, int bits);
    void fullTriBit(vluint32_t code, const vluint32_t newval, const vluint32_t newtri);
    void fullTriBus(vluint32_t code, const vluint32_t newval, const vluint32_t newtri, int bits);
    void fullTriQuad(vluint32_t code, const vluint64_t newval, const vluint32_t newtri, int bits);
    void fullTriArray(vluint32_t code, const vluint32_t* newvalp, const vluint32_t* newtrip,
                      int bits);
    void fullDouble(vluint32_t code, const double newval);
    void fullFloat(vluint32_t code, const float newval);

    /// Inside dumping routines, dump one signal as unknowns
    /// Presently this code doesn't change the oldval vector.
    /// Thus this is for special standalone applications that after calling
    /// fullBitX, must when then value goes non-X call fullBit.
    void fullBitX(vluint32_t code);
    void fullBusX(vluint32_t code, int bits);
    void fullQuadX(vluint32_t code, int bits);
    void fullArrayX(vluint32_t code, int bits);

    /// Inside dumping routines, dump one signal if it has changed.
    /// We do want to inline these to avoid calls when the value did not change.
    inline void chgBit(vluint32_t code, const vluint32_t newval) {
        vluint32_t diff = m_sigs_oldvalp[code] ^ newval;
        if (VL_UNLIKELY(diff)) fullBit(code, newval);
    }
    inline void chgBus(vluint32_t code, const vluint32_t newval, int bits) {
        vluint32_t diff = m_sigs_oldvalp[code] ^ newval;
        if (VL_UNLIKELY(diff)) {
            if (VL_UNLIKELY(bits == 32 || (diff & ((1U << bits) - 1)))) {
                fullBus(code, newval, bits);
            }
        }
    }
    inline void chgQuad(vluint32_t code, const vluint64_t newval, int bits) {
        vluint64_t diff = (*(reinterpret_cast<vluint64_t*>(&m_sigs_oldvalp[code]))) ^ newval;
        if (VL_UNLIKELY(diff)) {
            if (VL_UNLIKELY(bits == 64 || (diff & ((VL_ULL(1) << bits) - 1)))) {
                fullQuad(code, newval, bits);
            }
        }
    }
    inline void chgArray(vluint32_t code, const vluint32_t* newvalp, int bits) {
        for (int word = 0; word < (((bits - 1) / 32) + 1); ++word) {
            if (VL_UNLIKELY(m_sigs_oldvalp[code + word] ^ newvalp[word])) {
                fullArray(code, newvalp, bits);
                return;
            }
        }
    }
    inline void chgArray(vluint32_t code, const vluint64_t* newvalp, int bits) {
        for (int word = 0; word < (((bits - 1) / 64) + 1); ++word) {
            if (VL_UNLIKELY(m_sigs_oldvalp[code + word] ^ newvalp[word])) {
                fullArray(code, newvalp, bits);
                return;
            }
        }
    }
    inline void chgTriBit(vluint32_t code, const vluint32_t newval, const vluint32_t newtri) {
        vluint32_t diff = ((m_sigs_oldvalp[code] ^ newval) | (m_sigs_oldvalp[code + 1] ^ newtri));
        if (VL_UNLIKELY(diff)) {
            // Verilator 3.510 and newer provide clean input, so the below
            // is only for back compatibility
            if (VL_UNLIKELY(diff & 1)) {  // Change after clean?
                fullTriBit(code, newval, newtri);
            }
        }
    }
    inline void chgTriBus(vluint32_t code, const vluint32_t newval, const vluint32_t newtri,
                          int bits) {
        vluint32_t diff = ((m_sigs_oldvalp[code] ^ newval) | (m_sigs_oldvalp[code + 1] ^ newtri));
        if (VL_UNLIKELY(diff)) {
            if (VL_UNLIKELY(bits == 32 || (diff & ((1U << bits) - 1)))) {
                fullTriBus(code, newval, newtri, bits);
            }
        }
    }
    inline void chgTriQuad(vluint32_t code, const vluint64_t newval, const vluint32_t newtri,
                           int bits) {
        vluint64_t diff
            = (((*(reinterpret_cast<vluint64_t*>(&m_sigs_oldvalp[code]))) ^ newval)
               | ((*(reinterpret_cast<vluint64_t*>(&m_sigs_oldvalp[code + 1]))) ^ newtri));
        if (VL_UNLIKELY(diff)) {
            if (VL_UNLIKELY(bits == 64 || (diff & ((VL_ULL(1) << bits) - 1)))) {
                fullTriQuad(code, newval, newtri, bits);
            }
        }
    }
    inline void chgTriArray(vluint32_t code, const vluint32_t* newvalp, const vluint32_t* newtrip,
                            int bits) {
        for (int word = 0; word < (((bits - 1) / 32) + 1); ++word) {
            if (VL_UNLIKELY((m_sigs_oldvalp[code + word * 2] ^ newvalp[word])
                            | (m_sigs_oldvalp[code + word * 2 + 1] ^ newtrip[word]))) {
                fullTriArray(code, newvalp, newtrip, bits);
                return;
            }
        }
    }
    inline void chgDouble(vluint32_t code, const double newval) {
        // cppcheck-suppress invalidPointerCast
        if (VL_UNLIKELY((*(reinterpret_cast<double*>(&m_sigs_oldvalp[code]))) != newval)) {
            fullDouble(code, newval);
        }
    }
    inline void chgFloat(vluint32_t code, const float newval) {
        // cppcheck-suppress invalidPointerCast
        if (VL_UNLIKELY((*(reinterpret_cast<float*>(&m_sigs_oldvalp[code]))) != newval)) {
            fullFloat(code, newval);
        }
    }

#endif  // VL_TRACE_VCD_OLD_API

protected:
    // METHODS
    void evcd(bool flag) { m_evcd = flag; }
};

//=============================================================================
// VerilatedVcdC
/// Create a VCD dump file in C standalone (no SystemC) simulations.
/// Also derived for use in SystemC simulations.
/// Thread safety: Unless otherwise indicated, every function is VL_MT_UNSAFE_ONE

class VerilatedVcdC {
    VerilatedVcd m_sptrace;  ///< Trace file being created

    // CONSTRUCTORS
    VL_UNCOPYABLE(VerilatedVcdC);

public:
    explicit VerilatedVcdC(VerilatedVcdFile* filep = NULL)
        : m_sptrace(filep) {}
    ~VerilatedVcdC() { close(); }
    /// Routines can only be called from one thread; allow next call from different thread
    void changeThread() { spTrace()->changeThread(); }

public:
    // ACCESSORS
    /// Is file open?
    bool isOpen() const { return m_sptrace.isOpen(); }
    // METHODS
    /// Open a new VCD file
    /// This includes a complete header dump each time it is called,
    /// just as if this object was deleted and reconstructed.
    void open(const char* filename) VL_MT_UNSAFE_ONE { m_sptrace.open(filename); }
    /// Continue a VCD dump by rotating to a new file name
    /// The header is only in the first file created, this allows
    /// "cat" to be used to combine the header plus any number of data files.
    void openNext(bool incFilename = true) VL_MT_UNSAFE_ONE { m_sptrace.openNext(incFilename); }
    /// Set size in megabytes after which new file should be created
    void rolloverMB(size_t rolloverMB) { m_sptrace.rolloverMB(rolloverMB); }
    /// Close dump
    void close() VL_MT_UNSAFE_ONE { m_sptrace.close(); }
    /// Flush dump
    void flush() VL_MT_UNSAFE_ONE { m_sptrace.flush(); }
    /// Write one cycle of dump data
    void dump(vluint64_t timeui) { m_sptrace.dump(timeui); }
    /// Write one cycle of dump data - backward compatible and to reduce
    /// conversion warnings.  It's better to use a vluint64_t time instead.
    void dump(double timestamp) { dump(static_cast<vluint64_t>(timestamp)); }
    void dump(vluint32_t timestamp) { dump(static_cast<vluint64_t>(timestamp)); }
    void dump(int timestamp) { dump(static_cast<vluint64_t>(timestamp)); }
    /// Set time units (s/ms, defaults to ns)
    /// For Verilated models, these propage from the Verilated default --timeunit
    void set_time_unit(const char* unit) { m_sptrace.set_time_unit(unit); }
    void set_time_unit(const std::string& unit) { set_time_unit(unit.c_str()); }
    /// Set time resolution (s/ms, defaults to ns)
    /// For Verilated models, these propage from the Verilated default --timeunit
    void set_time_resolution(const char* unit) { m_sptrace.set_time_resolution(unit); }
    void set_time_resolution(const std::string& unit) { set_time_resolution(unit.c_str()); }

    /// Internal class access
    inline VerilatedVcd* spTrace() { return &m_sptrace; }
};

#endif  // guard
