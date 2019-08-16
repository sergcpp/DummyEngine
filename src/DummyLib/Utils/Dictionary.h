#pragma once

#include <istream>
#include <memory>

#include <Ren/HashMap32.h>
#include <Ren/Log.h>

enum class eGramGrpPos { Noun, Verb, Adjective };

enum class eGramGrpNum { Singular, Plural };

enum class eGramGrpGen { Masculine, Feminine, Neutral };

class Dictionary {
  public:
    bool Load(std::istream &in_data, Ren::ILog *log);

    struct dict_entry_res_t {
        eGramGrpPos pos;
        eGramGrpNum num;
        eGramGrpGen gen;
        const char *orth, *pron;
        const char *trans[16];
    };

    bool Lookup(const char *key, dict_entry_res_t &result);

    struct dict_link_compact_t {
        uint32_t key_str_off;
        uint32_t entry_index;
        uint32_t entry_count;
    };
    static_assert(sizeof(dict_link_compact_t) == 12, "!");

    struct dict_link_nokey_t {
        uint32_t entry_index;
        uint32_t entry_count;
    };

    struct dict_entry_compact_t {
        uint8_t pos;
        uint8_t num;
        uint8_t gen;
        uint8_t trans_count;
        uint32_t orth_str_off;
        uint32_t pron_str_off;
        uint32_t trans_str_off;
    };
    static_assert(sizeof(dict_entry_compact_t) == 16, "!");

    enum class eDictChunks { DictChInfo, DictChLinks, DictChEntries, DictChStrings, DictChCount };

    struct dict_info_t {
        char src_lang[2], dst_lang[2];
        uint32_t keys_count, entries_count;
    };
    static_assert(sizeof(dict_info_t) == 12, "!");

  private:
    dict_info_t info_;
    std::unique_ptr<dict_entry_compact_t[]> entries_;
    std::unique_ptr<char[]> comb_str_buf_;
    Ren::HashMap32<const char *, dict_link_nokey_t> hashmap_;
};