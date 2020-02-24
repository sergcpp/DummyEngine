#include "Dictionary.h"

bool Dictionary::Lookup(const char *key, dict_entry_res_t &result) {
    const dict_link_nokey_t *link = hashmap_.Find(key);
    if (link) {
        for (uint32_t i = link->entry_index; i < link->entry_index + link->entry_count; i++) {
            const dict_entry_compact_t &entry = entries_[i];

            result.pos = (eGramGrpPos)entry.pos;
            result.num = (eGramGrpNum)entry.num;
            result.gen = (eGramGrpGen)entry.gen;

            result.orth = &comb_str_buf_[entry.orth_str_off];
            result.pron = entry.pron_str_off != 0xffffffff ? &comb_str_buf_[entry.pron_str_off] : nullptr;

            uint32_t j = 0, str_off = entry.trans_str_off;
            while (j < entry.trans_count && j < (sizeof(result.trans) / sizeof(const char *))) {
                result.trans[j++] = &comb_str_buf_[str_off];

                // skip to next string
                while (comb_str_buf_[str_off++]);
            }
        }

        return true;
    } else {
        return false;
    }
}

bool Dictionary::Load(std::istream &in_data, Ren::ILog *log) {
    char signature[4];
    in_data.read(signature, 4);

    if (signature[0] != 'D' || signature[1] != 'I' || signature[2] != 'C' || signature[3] != 'T') {
        log->Error("Wrong file signature!");
        return false;
    }

    uint32_t header_size;
    in_data.read((char *)&header_size, sizeof(uint32_t));

    const int chunks_count = int(header_size - sizeof(uint32_t) - 4) / int(3 * sizeof(uint32_t));

    std::unique_ptr<dict_link_compact_t[]> temp_links;

    for (int i = 0; i < chunks_count; i++) {
        uint32_t chunk_id, chunk_offset, chunk_size;
        in_data.read((char *)&chunk_id, sizeof(uint32_t));
        in_data.read((char *)&chunk_offset, sizeof(uint32_t));
        in_data.read((char *)&chunk_size, sizeof(uint32_t));

        const size_t file_offset = (size_t)in_data.tellg();

        in_data.seekg(chunk_offset, std::ios::beg);

        if (chunk_id == DictChInfo) {
            if (chunk_size != sizeof(dict_info_t)) {
                log->Error("Info chunk size does not match!");
                return false;
            }
            in_data.read((char *)&info_, sizeof(dict_info_t));
        } else if (chunk_id == DictChLinks) {
            if ((chunk_size % sizeof(dict_link_compact_t)) != 0) {
                log->Error("Wrong links chunk size!");
                return false;
            }
            const uint32_t links_count = chunk_size / sizeof(dict_link_compact_t);
            temp_links.reset(new dict_link_compact_t[links_count]);
            in_data.read((char *)temp_links.get(), chunk_size);
        } else if (chunk_id == DictChEntries) {
            if ((chunk_size % sizeof(dict_entry_compact_t)) != 0) {
                log->Error("Wrong entries chunk size!");
                return false;
            }
            const uint32_t entries_count = chunk_size / sizeof(dict_entry_compact_t);
            entries_.reset(new dict_entry_compact_t[entries_count]);
            in_data.read((char *)entries_.get(), chunk_size);
        } else if (chunk_id == DictChStrings) {
            comb_str_buf_.reset(new char[chunk_size]);
            in_data.read(comb_str_buf_.get(), chunk_size);
        }

        in_data.seekg(file_offset, std::ios::beg);
    }

    hashmap_.clear();
    hashmap_.reserve(info_.keys_count);

    for (int i = 0; i < info_.keys_count; i++) {
        const dict_link_compact_t &link = temp_links[i];

        const char *key = &comb_str_buf_[link.key_str_off];

        dict_link_nokey_t *hlink = hashmap_.InsertNoCheck(key);
        hlink->entry_index = link.entry_index;
        hlink->entry_count = link.entry_count;
    }

    return true;
}