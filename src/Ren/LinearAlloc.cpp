#include "LinearAlloc.h"

#include "Log.h"

int Ren::LinearAlloc::Alloc_r(const int i, const uint32_t req_size, const char *tag) {
    if (!nodes_[i].is_free || req_size > nodes_[i].size) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int new_node = Alloc_r(ch0, req_size, tag);
        if (new_node != -1) {
            return new_node;
        }

        return Alloc_r(ch1, req_size, tag);
    } else {
        if (req_size == nodes_[i].size) {
#ifndef NDEBUG
            strncpy(nodes_[i].tag, tag, 31);
#endif
            nodes_[i].is_free = false;
            return i;
        }

        nodes_[i].child[0] = ch0 = nodes_.emplace();
        nodes_[i].child[1] = ch1 = nodes_.emplace();

        Node &n = nodes_[i];

        nodes_[ch0].offset = n.offset;
        nodes_[ch0].size = req_size;
        nodes_[ch1].offset = n.offset + req_size;
        nodes_[ch1].size = n.size - req_size;
        nodes_[ch0].parent = nodes_[ch1].parent = i;

        return Alloc_r(ch0, req_size, tag);
    }
}

int Ren::LinearAlloc::Find_r(const int i, const uint32_t offset) const {
    if ((nodes_[i].is_free && !nodes_[i].has_children()) || offset < nodes_[i].offset ||
        offset > (nodes_[i].offset + nodes_[i].size)) {
        return -1;
    }

    const int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int ndx = Find_r(ch0, offset);
        if (ndx != -1) {
            return ndx;
        }
        return Find_r(ch1, offset);
    } else {
        if (offset == nodes_[i].offset) {
            return i;
        } else {
            return -1;
        }
    }
}

bool Ren::LinearAlloc::Free_Node(int i) {
    if (i == -1 || nodes_[i].is_free) {
        return false;
    }

    nodes_[i].is_free = true;

    int par = nodes_[i].parent;
    while (par != -1) {
        int ch0 = nodes_[par].child[0], ch1 = nodes_[par].child[1];

        if (!nodes_[ch0].has_children() && nodes_[ch0].is_free && !nodes_[ch1].has_children() && nodes_[ch1].is_free) {

            nodes_.erase(ch0);
            nodes_.erase(ch1);

            nodes_[par].child[0] = nodes_[par].child[1] = -1;

            i = par;
            par = nodes_[par].parent;
        } else {
            par = -1;
        }
    }

    { // merge empty nodes
        int par = nodes_[i].parent;
        while (par != -1 && nodes_[par].child[0] == i && !nodes_[i].has_children()) {
            int gr_par = nodes_[par].parent;
            if (gr_par != -1 && nodes_[gr_par].has_children()) {
                int ch0 = nodes_[gr_par].child[0], ch1 = nodes_[gr_par].child[1];

                if (!nodes_[ch0].has_children() && nodes_[ch0].is_free && ch1 == par) {
                    assert(nodes_[ch0].offset + nodes_[ch0].size == nodes_[i].offset);
                    nodes_[ch0].size += nodes_[i].size;
                    nodes_[gr_par].child[1] = nodes_[par].child[1];
                    nodes_[nodes_[par].child[1]].parent = gr_par;

                    nodes_.erase(i);
                    nodes_.erase(par);

                    i = ch0;
                    par = gr_par;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    return true;
}

void Ren::LinearAlloc::PrintNode(int i, std::string prefix, bool is_tail, ILog *log) const {
    const auto &node = nodes_[i];
    if (is_tail) {
        if (!node.has_children() && node.is_free) {
            log->Info("%s+- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s+- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s+- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "   ";
    } else {
        if (!node.has_children() && node.is_free) {
            log->Info("%s|- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s|- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s|- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "|  ";
    }

    if (node.child[0] != -1) {
        PrintNode(node.child[0], prefix, false, log);
    }

    if (node.child[1] != -1) {
        PrintNode(node.child[1], prefix, true, log);
    }
}

void Ren::LinearAlloc::Clear() { nodes_.clear(); }