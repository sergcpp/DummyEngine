#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/Time_.h>

#include "TexUpdateFileBuf.h"

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {
extern const char *TEXTURES_PATH;

__itt_string_handle *itt_read_file_str = __itt_string_handle_create("ReadFile");
__itt_string_handle *itt_sort_tex_str = __itt_string_handle_create("SortTextures");

const size_t TextureMemoryLimit = 768 * 1024 * 1024;
} // namespace SceneManagerConstants

namespace SceneManagerInternal {
void ParseDDSHeader(const Ren::DDSHeader &hdr, Ren::Tex2DParams &params, Ren::ILog *log) {
    params.w = uint16_t(hdr.dwWidth);
    params.h = uint16_t(hdr.dwHeight);
    params.mip_count = uint8_t(hdr.dwMipMapCount);

    const int px_format = int(hdr.sPixelFormat.dwFourCC >> 24u) - '0';
    switch (px_format) {
    case 1:
        params.format = Ren::eTexFormat::DXT1;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 3:
        params.format = Ren::eTexFormat::DXT3;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 5:
        params.format = Ren::eTexFormat::DXT5;
        params.block = Ren::eTexBlock::_4x4;
        break;
    default:
        params.block = Ren::eTexBlock::_None;
        if (hdr.sPixelFormat.dwFlags & DDPF_RGB) {
            // Uncompressed
            if (hdr.sPixelFormat.dwRGBBitCount == 32) {
                params.format = Ren::eTexFormat::RawRGBA8888;
            } else if (hdr.sPixelFormat.dwRGBBitCount == 24) {
                params.format = Ren::eTexFormat::RawRGB888;
            } else {
                params.format = Ren::eTexFormat::Undefined;
            }
        } else {
            // Possibly need to read DX10 header
            params.format = Ren::eTexFormat::Undefined;
        }
        return;
    }
}

void CaptureMaterialTextureChange(Ren::Context &ctx, SceneData &scene_data, const Ren::Tex2DRef &ref) {
    uint32_t tex_user = ref->first_user;
    while (tex_user != 0xffffffff) {
        Ren::Material &mat = scene_data.materials[tex_user];
        scene_data.material_changes.push_back(tex_user);
        const size_t ndx =
            std::distance(mat.textures.begin(), std::find(mat.textures.begin(), mat.textures.end(), ref));
        Ren::eSamplerLoadStatus status;
        mat.samplers[ndx] = ctx.LoadSampler(ref->sampling(), &status);
        tex_user = mat.next_texture_user[ndx];
    }
}
} // namespace SceneManagerInternal

void SceneManager::TextureLoaderProc() {
    using namespace SceneManagerConstants;
    using namespace SceneManagerInternal;

    __itt_thread_set_name("Texture loader");
    OPTICK_FRAME("Texture loader");

    int iteration = 0;

    const size_t SortPortion = 16;
    const int SortInterval = 8;

    for (;;) {
        TextureRequestPending *req = nullptr;

        {
            std::unique_lock<std::mutex> lock(tex_requests_lock_);
            tex_loader_cnd_.wait(lock, [this, &req] {
                if (tex_loader_stop_) {
                    return true;
                }

                if (requested_textures_.empty() || scene_data_.estimated_texture_mem.load() > TextureMemoryLimit) {
                    return false;
                }

                for (int i = 0; i < MaxSimultaneousRequests; i++) {
                    if (io_pending_tex_[i].state == eRequestState::Idle) {
                        return true;
                    }
                }

                return false;
            });
            if (tex_loader_stop_) {
                break;
            }

            OPTICK_EVENT("Texture Request");

            for (int i = 0; i < MaxSimultaneousRequests; i++) {
                if (io_pending_tex_[i].state == eRequestState::Idle) {
                    req = &io_pending_tex_[i];
                    break;
                }
            }

            if (!req) {
                continue;
            }

            /**/

            if (iteration++ % SortInterval == 0) {
                __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_sort_tex_str);
                if (SortPortion != -1) {
                    const size_t sort_portion = std::min(SortPortion, requested_textures_.size());
                    std::partial_sort(std::begin(requested_textures_), std::begin(requested_textures_) + sort_portion,
                                      std::end(requested_textures_),
                                      [](const TextureRequest &lhs, const TextureRequest &rhs) {
                                          return lhs.sort_key < rhs.sort_key;
                                      });
                } else {
                    std::sort(std::begin(requested_textures_), std::end(requested_textures_),
                              [](const TextureRequest &lhs, const TextureRequest &rhs) {
                                  return lhs.sort_key < rhs.sort_key;
                              });
                }
                __itt_task_end(__g_itt_domain);
            }

            static_cast<TextureRequest &>(*req) = requested_textures_.front();
            requested_textures_.pop_front();
        }

        __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_read_file_str);
        OPTICK_EVENT("Read File");

        req->buf->set_data_len(0);
        req->mip_offset_to_init = 0xff;

        size_t read_offset = 0, read_size = 0;

        char path_buf[4096];
        strcpy(path_buf, SceneManagerConstants::TEXTURES_PATH);
        strcat(path_buf, req->ref->name().c_str());

        bool read_success = true;

        if (req->ref->name().EndsWith(".dds") || req->ref->name().EndsWith(".DDS")) {
            if (req->orig_format == Ren::eTexFormat::Undefined) {
                Ren::DDSHeader header;

                size_t data_size = sizeof(Ren::DDSHeader);
                read_success = tex_reader_.ReadFileBlocking(path_buf, 0 /* file_offset */, sizeof(Ren::DDSHeader),
                                                            &header, data_size);
                read_success &= (data_size == sizeof(Ren::DDSHeader));

                req->read_offset = sizeof(Ren::DDSHeader);

                if (read_success) {
                    Ren::Tex2DParams temp_params;
                    ParseDDSHeader(header, temp_params, ren_ctx_.log());
                    req->orig_format = temp_params.format;
                    req->orig_block = temp_params.block;
                    req->orig_w = temp_params.w;
                    req->orig_h = temp_params.h;
                    req->orig_mip_count = int(header.dwMipMapCount);

                    if (header.sPixelFormat.dwFourCC == ((unsigned('D') << 0u) | (unsigned('X') << 8u) |
                                                         (unsigned('1') << 16u) | (unsigned('0') << 24u))) {
                        Ren::DDS_HEADER_DXT10 dx10_header;
                        tex_reader_.ReadFileBlocking(path_buf, sizeof(Ren::DDSHeader), sizeof(Ren::DDS_HEADER_DXT10),
                                                     &dx10_header, data_size);

                        req->orig_format = Ren::TexFormatFromDXGIFormat(dx10_header.dxgiFormat);

                        read_offset += sizeof(Ren::DDS_HEADER_DXT10);
                    }
                }
            }
            read_offset += req->read_offset;
        } else if (req->ref->name().EndsWith(".ktx") || req->ref->name().EndsWith(".KTX")) {
            assert(false && "Not implemented!");
            read_offset += sizeof(Ren::KTXHeader);
        } else {
            read_success = false;
        }

        if (read_success) {
            const Ren::Tex2DParams &cur_p = req->ref->params;

            const int max_load_w = std::max(cur_p.w * 2, 64);
            const int max_load_h = std::max(cur_p.h * 2, 64);

            int w = int(req->orig_w), h = int(req->orig_h);
            for (int i = 0; i < req->orig_mip_count; i++) {
                const int data_len = Ren::GetMipDataLenBytes(w, h, req->orig_format, req->orig_block);

                if ((w <= max_load_w && h <= max_load_h) || i == req->orig_mip_count - 1) {
                    if (req->mip_offset_to_init == 0xff) {
                        req->mip_offset_to_init = uint8_t(i);
                        req->mip_count_to_init = 0;
                    }
                    if (w > cur_p.w || h > cur_p.h || (cur_p.mip_count == 1 && w == cur_p.w && h == cur_p.h)) {
                        req->mip_count_to_init++;
                        read_size += data_len;
                    }
                } else {
                    read_offset += data_len;
                }

                w = std::max(w / 2, 1);
                h = std::max(h / 2, 1);
            }

            // load next mip levels
            assert(req->ref->params.w == req->orig_w || req->ref->params.h != req->orig_h);

            if (read_size) {
                read_success = tex_reader_.ReadFileNonBlocking(path_buf, read_offset, read_size, *req->buf, req->ev);
                assert(req->buf->data_len() == read_size);
            }
        }

        if (read_success) {
            std::lock_guard<std::mutex> _(tex_requests_lock_);
            req->state = eRequestState::PendingIO;
        }

        __itt_task_end(__g_itt_domain);
    }
}

void SceneManager::EstimateTextureMemory(const int portion_size) {
    OPTICK_EVENT();
    if (scene_data_.textures.capacity() == 0) {
        return;
    }

    const int BucketSize = 16;
    scene_data_.texture_mem_buckets.resize((scene_data_.textures.capacity() + BucketSize - 1) / BucketSize);

    uint64_t mem_after_estimation = scene_data_.estimated_texture_mem.load();

    for (int i = 0; i < portion_size; i++) {
        scene_data_.tex_mem_bucket_index =
            (scene_data_.tex_mem_bucket_index + 1) % scene_data_.texture_mem_buckets.size();
        uint32_t &bucket = scene_data_.texture_mem_buckets[scene_data_.tex_mem_bucket_index];
        mem_after_estimation -= bucket;
        bucket = 0;

        const uint32_t start = scene_data_.tex_mem_bucket_index * BucketSize;
        const uint32_t end = std::min(start + BucketSize, uint32_t(scene_data_.textures.capacity()));

        uint32_t index = scene_data_.textures.FindOccupiedInRange(start, end);
        while (index != end) {
            const auto &tex = scene_data_.textures.at(index);

            bucket += Ren::EstimateMemory(tex.params);

            index = scene_data_.textures.FindOccupiedInRange(index + 1, end);
        }

        mem_after_estimation += bucket;
    }

    if (scene_data_.estimated_texture_mem.exchange(mem_after_estimation) != mem_after_estimation) {
        ren_ctx_.log()->Info("Estimated tex memory is %.3f MB", double(mem_after_estimation) / (1024.0f * 1024.0f));

        std::lock_guard<std::mutex> _(tex_requests_lock_);
        tex_loader_cnd_.notify_one();
    }
}

void SceneManager::ProcessPendingTextures(const int portion_size) {
    using namespace SceneManagerConstants;

    OPTICK_GPU_EVENT("ProcessPendingTextures");

    //
    // Process io pending textures
    //
    for (int i = 0; i < portion_size; i++) {
        OPTICK_EVENT("Process io pending textures");

        TextureRequestPending *req = nullptr;
        Sys::eFileReadResult res;
        size_t bytes_read = 0;
        {
            std::lock_guard<std::mutex> _(tex_requests_lock_);
            for (int i = 0; i < MaxSimultaneousRequests; i++) {
                if (io_pending_tex_[i].state == eRequestState::PendingIO) {
                    res = io_pending_tex_[i].ev.GetResult(false /* block */, &bytes_read);
                    if (res != Sys::eFileReadResult::Pending) {
                        req = &io_pending_tex_[i];
                        break;
                    }
                } else if (io_pending_tex_[i].state == eRequestState::PendingUpdate) {
                    TextureRequestPending *req = &io_pending_tex_[i];
                    auto *stage_buf = static_cast<TextureUpdateFileBuf *>(req->buf.get());
                    const auto res = stage_buf->fence.ClientWaitSync(0 /* timeout_us */);
                    if (res == Ren::WaitResult::Fail) {
                        ren_ctx_.log()->Error("Waiting on fence failed!");

                        static_cast<TextureRequest &>(*req) = {};
                        req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    } else if (res != Ren::WaitResult::Timeout) {
                        if (req->ref->params.sampling.min_lod.value() > 0) {
                            const auto it =
                                std::lower_bound(std::begin(lod_transit_textures_), std::end(lod_transit_textures_),
                                                 req->ref, [](const Ren::Tex2DRef &lhs, const Ren::Tex2DRef &rhs) {
                                                     return lhs.index() < rhs.index();
                                                 });
                            if (it == std::end(lod_transit_textures_) || it->index() != req->ref.index()) {
                                lod_transit_textures_.insert(it, {req->ref});
                            }
                        }

                        if (req->ref->params.w != req->orig_w || req->ref->params.h != req->orig_h) {
                            // process texture further (for next mip levels)
                            req->sort_key = req->ref->name().StartsWith("lightmap") ? 0 : 0xffffffff;
                            requested_textures_.push_back(std::move(*req));
                        } else {
                            std::lock_guard<std::mutex> _lock(gc_textures_mtx_);
                            finished_textures_.push_back(std::move(*req));
                        }

                        static_cast<TextureRequest &>(*req) = {};
                        req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    }
                }
            }
        }

        if (req) {
            const Ren::String &tex_name = req->ref->name();

            OPTICK_GPU_EVENT("Process pending texture");

            if (res == Sys::eFileReadResult::Successful) {
                assert(dynamic_cast<TextureUpdateFileBuf *>(req->buf.get()));
                auto *stage_buf = static_cast<TextureUpdateFileBuf *>(req->buf.get());

                stage_buf->stage_buf().FlushMappedRange(0, stage_buf->stage_buf().AlignMapOffset(uint32_t(bytes_read)));

                const uint64_t t1_us = Sys::GetTimeUs();

                int w = std::max(int(req->orig_w) >> req->mip_offset_to_init, 1);
                int h = std::max(int(req->orig_h) >> req->mip_offset_to_init, 1);

                uint16_t initialized_mips = req->ref->initialized_mips();
                int last_initialized_mip = 0;
                for (uint16_t temp = initialized_mips; temp >>= 1; ++last_initialized_mip)
                    ;

                const Ren::Tex2DParams &p = req->ref->params;

                // stage_buf->fence.ClientWaitSync();
                ren_ctx_.BegSingleTimeCommands(stage_buf->cmd_buf);

                req->ref->Realloc(w, h, last_initialized_mip + 1 + req->mip_count_to_init, 1 /* samples */,
                                  req->orig_format, req->orig_block, bool(p.flags & Ren::eTexFlagBits::SRGB),
                                  stage_buf->cmd_buf, ren_ctx_.default_mem_allocs(), ren_ctx_.log());

                initialized_mips = req->ref->initialized_mips();

                int data_off = int(req->buf->data_off());
                for (int i = int(req->mip_offset_to_init); i < int(req->mip_offset_to_init) + req->mip_count_to_init;
                     i++) {
                    if (data_off >= int(bytes_read)) {
                        ren_ctx_.log()->Error("File %s has not enough data!", tex_name.c_str());
                        break;
                    }
                    const int data_len = Ren::GetMipDataLenBytes(w, h, req->orig_format, req->orig_block);

                    const int mip_index = i - req->mip_offset_to_init;
                    if ((initialized_mips & (1u << mip_index)) == 0) {
                        req->ref->SetSubImage(mip_index, 0, 0, w, h, req->orig_format, stage_buf->stage_buf(),
                                              stage_buf->cmd_buf, data_off, data_len);
                    }

                    data_off += data_len;
                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                }

                stage_buf->fence = ren_ctx_.EndSingleTimeCommands(stage_buf->cmd_buf);

                { // offset min lod to account for newly allocated mip levels
                    Ren::SamplingParams cur_sampling = req->ref->params.sampling;
                    cur_sampling.min_lod.set_value(cur_sampling.min_lod.value() +
                                                   cur_sampling.min_lod.One * req->mip_count_to_init);
                    req->ref->SetSampling(cur_sampling);
                    SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, req->ref);
                }

                const uint64_t t2_us = Sys::GetTimeUs();

                ren_ctx_.log()->Info("Texture %s loaded (%.3f ms)", tex_name.c_str(), double(t2_us - t1_us) * 0.001);
            } else if (res == Sys::eFileReadResult::Failed) {
                ren_ctx_.log()->Error("Error loading %s", tex_name.c_str());
            }

            if (res != Sys::eFileReadResult::Pending) {
                if (res == Sys::eFileReadResult::Successful) {
                    req->state = eRequestState::PendingUpdate;
                } else {
                    static_cast<TextureRequest &>(*req) = {};

                    std::lock_guard<std::mutex> _(tex_requests_lock_);
                    req->state = eRequestState::Idle;
                    tex_loader_cnd_.notify_one();
                }
            } else {
                break;
            }
        }
    }

    { // Animate lod transition to avoid sudden popping
        OPTICK_EVENT("lod transition");
        for (auto it = std::begin(lod_transit_textures_); it != std::end(lod_transit_textures_);) {
            Ren::SamplingParams cur_sampling = (*it)->params.sampling;
            cur_sampling.min_lod.set_value(std::max(cur_sampling.min_lod.value() - 1, 0));
            (*it)->SetSampling(cur_sampling);

            SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, *it);

            if (cur_sampling.min_lod.value() == 0) {
                // transition is done, remove texture from list
                it = lod_transit_textures_.erase(it);
            } else {
                ++it;
            }
        }
    }

    { // Process GC'ed textures
        std::lock_guard<std::mutex> lock(gc_textures_mtx_);
        OPTICK_GPU_EVENT("GC textures");
        for (int i = 0; i < portion_size && !gc_textures_.empty(); i++) {
            auto &req = gc_textures_.front();

            ren_ctx_.log()->Warning("Texture %s is being garbage collected", req.ref->name().c_str());

            Ren::Tex2DParams p = req.ref->params;

            // drop to lowest lod
            const int w = std::max(p.w >> p.mip_count, 1);
            const int h = std::max(p.h >> p.mip_count, 1);

            req.ref->Realloc(w, h, 1 /* mip_count */, 1 /* samples */, p.format, p.block,
                             bool(p.flags & Ren::eTexFlagBits::SRGB), ren_ctx_.current_cmd_buf(),
                             ren_ctx_.default_mem_allocs(), ren_ctx_.log());

            p.sampling.min_lod.from_float(-1.0f);
            req.ref->SetSampling(p.sampling);

            SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, req.ref);

            { // send texture for processing
                std::lock_guard<std::mutex> _lock(tex_requests_lock_);
                requested_textures_.push_back(req);
            }

            gc_textures_.pop_front();
        }
    }
}

void SceneManager::RebuildMaterialTextureGraph() {
    OPTICK_EVENT();

    // reset texture user
    for (auto &texture : scene_data_.textures) {
        texture.first_user = 0xffffffff;
    }
    // assign material index as the first user
    for (auto it = scene_data_.materials.begin(); it != scene_data_.materials.end(); ++it) {
        it->next_texture_user = {};
        it->next_texture_user.resize(it->textures.size(), 0xffffffff);
        for (auto &texture : it->textures) {
            if (texture->first_user == 0xffffffff) {
                texture->first_user = it.index();
            } else {
                uint32_t last_user;
                uint32_t next_user = texture->first_user;
                do {
                    const auto &mat = scene_data_.materials.at(next_user);
                    const size_t ndx = std::distance(mat.textures.begin(),
                                                     std::find(mat.textures.begin(), mat.textures.end(), texture));
                    last_user = next_user;
                    next_user = mat.next_texture_user[ndx];
                } while (next_user != 0xffffffff);

                { // set next user
                    auto &mat = scene_data_.materials.at(last_user);
                    const size_t ndx = std::distance(mat.textures.begin(),
                                                     std::find(mat.textures.begin(), mat.textures.end(), texture));
                    assert(mat.next_texture_user[ndx] == 0xffffffff);
                    mat.next_texture_user[ndx] = it.index();
                }
            }
        }
    }
}

void SceneManager::UpdateTexturePriorities(const TexEntry visible_textures[], const int visible_count,
                                           const TexEntry desired_textures[], const int desired_count) {
    OPTICK_EVENT();

    TexturesGCIteration(visible_textures, visible_count, desired_textures, desired_count);

    { // Update requested textures
        std::unique_lock<std::mutex> lock(tex_requests_lock_);

        bool kick_loader_thread = false;
        for (auto it = requested_textures_.begin(); it != requested_textures_.end(); ++it) {
            const TexEntry *found_entry = nullptr;

            { // search among visible textures first
                const TexEntry *beg = visible_textures;
                const TexEntry *end = visible_textures + visible_count;

                const TexEntry *entry = std::lower_bound(
                    beg, end, it->ref.index(), [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

                if (entry != end && entry->index == it->ref.index()) {
                    found_entry = entry;
                }
            }

            if (!found_entry) { // search among surrounding textures
                const TexEntry *beg = desired_textures;
                const TexEntry *end = desired_textures + desired_count;

                const TexEntry *entry = std::lower_bound(
                    beg, end, it->ref.index(), [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

                if (entry != end && entry->index == it->ref.index()) {
                    found_entry = entry;
                }
            }

            if (found_entry) {
                it->sort_key = found_entry->sort_key;
                kick_loader_thread = true;
            }
        }

        if (kick_loader_thread) {
            tex_loader_cnd_.notify_one();
        }
    }
}

void SceneManager::TexturesGCIteration(const TexEntry visible_textures[], const int visible_count,
                                       const TexEntry desired_textures[], const int desired_count) {
    using namespace SceneManagerConstants;

    OPTICK_EVENT();

    const int FinishedPortion = 16;

    const bool enable_gc = (scene_data_.estimated_texture_mem.load() >= 9 * TextureMemoryLimit / 10);

    std::lock_guard<std::mutex> _lock(gc_textures_mtx_);

    auto start_it = finished_textures_.begin() + std::min(finished_index_, uint32_t(finished_textures_.size()));
    finished_index_ += FinishedPortion;
    auto end_it = finished_textures_.begin() + std::min(finished_index_, uint32_t(finished_textures_.size()));

    if (finished_index_ >= finished_textures_.size()) {
        finished_index_ = 0;
    }

    for (auto it = start_it; it != end_it;) {
        const TexEntry *found_entry = nullptr;

        { // search among visible textures first
            const TexEntry *beg = visible_textures;
            const TexEntry *end = visible_textures + visible_count;

            const TexEntry *entry = std::lower_bound(
                beg, end, it->ref.index(), [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

            if (entry != end && entry->index == it->ref.index()) {
                found_entry = entry;
            }
        }

        if (!found_entry) { // search among surrounding textures
            const TexEntry *beg = desired_textures;
            const TexEntry *end = desired_textures + desired_count;

            const TexEntry *entry = std::lower_bound(
                beg, end, it->ref.index(), [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });

            if (entry != end && entry->index == it->ref.index()) {
                found_entry = entry;
            }
        }

        if (found_entry) {
            it->sort_key = found_entry->sort_key;
            it->frame_dist = 0;

            ++it;
        } else if (++it->frame_dist > 1000 && enable_gc && it->ref->params.w > 16 && it->ref->params.w == it->orig_w &&
                   it->ref->params.h > 16 && it->ref->params.h == it->orig_h &&
                   !it->ref->name().StartsWith("lightmap")) {
            // Reduce texture's mips
            it->frame_dist = 0;
            it->sort_key = 0xffffffff;

            gc_textures_.push_back(*it);

            it = finished_textures_.erase(it);
        } else {
            ++it;
        }
    }
}

void SceneManager::InvalidateTexture(const Ren::Tex2DRef &ref) {
    SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, ref);
}

void SceneManager::StartTextureLoader() {
    tex_loader_stop_ = false;
    tex_loader_thread_ = std::thread(&SceneManager::TextureLoaderProc, this);
}

void SceneManager::StopTextureLoader() {
    { // stop texture loading thread
        std::unique_lock<std::mutex> lock(tex_requests_lock_);
        tex_loader_stop_ = true;
        tex_loader_cnd_.notify_one();
    }

    assert(tex_loader_thread_.joinable());
    tex_loader_thread_.join();
    for (int i = 0; i < MaxSimultaneousRequests; i++) {
        size_t bytes_read = 0;
        io_pending_tex_[i].ev.GetResult(true /* block */, &bytes_read);
        io_pending_tex_[i].state = eRequestState::Idle;
        io_pending_tex_[i].ref = {};
    }
    requested_textures_.clear();
    lod_transit_textures_.clear();
    {
        std::unique_lock<std::mutex> lock(gc_textures_mtx_);
        finished_textures_.clear();
        gc_textures_.clear();
    }
}

void SceneManager::ForceTextureReload() {
    StopTextureLoader();

    std::vector<Ren::TransitionInfo> img_transitions;
    img_transitions.reserve(scene_data_.textures.size());

    // Reset textures to 1x1 mip and send to processing
    for (auto it = std::begin(scene_data_.textures); it != std::end(scene_data_.textures); ++it) {
        Ren::Tex2DParams p = it->params;

        // drop to lowest lod
        const int w = std::max(p.w >> p.mip_count, 1);
        const int h = std::max(p.h >> p.mip_count, 1);

        it->Realloc(w, h, 1 /* mip_count */, 1 /* samples */, p.format, p.block,
                    bool(p.flags & Ren::eTexFlagBits::SRGB), ren_ctx_.current_cmd_buf(), ren_ctx_.default_mem_allocs(),
                    ren_ctx_.log());

        img_transitions.emplace_back(&(*it), Ren::eResState::ShaderResource);

        p.sampling.min_lod.from_float(-1.0f);
        it->ApplySampling(p.sampling, ren_ctx_.log());

        TextureRequest req;
        req.ref = Ren::Tex2DRef{&scene_data_.textures, it.index()};

        SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, req.ref);

        requested_textures_.push_back(std::move(req));
    }

    if (!img_transitions.empty()) {
        Ren::TransitionResourceStates(ren_ctx_.current_cmd_buf(), Ren::AllStages, Ren::AllStages, img_transitions);
    }

    std::fill(std::begin(scene_data_.texture_mem_buckets), std::end(scene_data_.texture_mem_buckets), 0);
    scene_data_.tex_mem_bucket_index = 0;
    scene_data_.estimated_texture_mem = 0;

    StartTextureLoader();
}

#undef NEXT_REQ_NDX
#undef PREV_REQ_NDX
