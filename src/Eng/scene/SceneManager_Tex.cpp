#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/utils/Utils.h>
#include <Sys/Time_.h>

#include "TexUpdateFileBuf.h"

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace Eng::SceneManagerConstants {
__itt_string_handle *itt_read_file_str = __itt_string_handle_create("ReadFile");
__itt_string_handle *itt_sort_tex_str = __itt_string_handle_create("SortTextures");
} // namespace Eng::SceneManagerConstants

namespace Eng::SceneManagerInternal {
void CaptureMaterialTextureChange(Ren::Context &ctx, Eng::SceneData &scene_data, const Ren::ImageHandle old_handle,
                                  const Ren::ImageHandle new_handle) {
    const Ren::StoragesRef &storages = ctx.storages();
    const auto &[img_main, img_cold] = storages.images[new_handle];

    uint32_t tex_user = storages.images[old_handle].second.first_user;
    img_cold.first_user = tex_user;
    while (tex_user != 0xffffffff) {
        const auto &[mat_main, mat_cold] = storages.materials.GetUnsafe(tex_user);
        scene_data.material_changes.push_back(tex_user);
        for (int i = 0; i < int(mat_main.textures.size()); ++i) {
            if (mat_main.textures[i] == old_handle) {
                mat_main.textures[i] = new_handle;

                const auto it = lower_bound(
                    std::begin(scene_data.samplers), std::end(scene_data.samplers), img_cold.params.sampling,
                    [&storages](const Ren::SamplerHandle lhs_handle, const Ren::SamplingParams s) {
                        return storages.samplers[lhs_handle].params < s;
                    });
                if (it == std::end(scene_data.samplers) || storages.samplers[*it].params != img_cold.params.sampling) {
                    mat_main.samplers[i] = ctx.CreateSampler(img_cold.params.sampling);
                    scene_data.samplers.insert(it, mat_main.samplers[i]);
                } else {
                    mat_main.samplers[i] = *it;
                }

                tex_user = mat_cold.next_texture_user[i];
            }
        }
    }
}
} // namespace Eng::SceneManagerInternal

void Eng::SceneManager::TextureLoaderProc() {
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
            tex_loader_cnd_.wait(lock, [this] {
                if (tex_loader_stop_) {
                    return true;
                }

                if (requested_textures_.empty() ||
                    scene_data_.estimated_texture_mem.load() > tex_memory_limit_.load()) {
                    return false;
                }

                for (TextureRequestPending &r : io_pending_tex_) {
                    if (r.state == eRequestState::Idle) {
                        return true;
                    }
                }

                return false;
            });
            if (tex_loader_stop_) {
                break;
            }

            OPTICK_EVENT("Texture Request");

            for (TextureRequestPending &r : io_pending_tex_) {
                if (r.state == eRequestState::Idle) {
                    req = &r;
                    break;
                }
            }

            if (!req) {
                continue;
            }

            /**/

            if (iteration++ % SortInterval == 0) {
                __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_sort_tex_str);
                if constexpr (SortPortion != -1) {
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

            assert(!req->img);
            req->state = eRequestState::InProgress;
            static_cast<TextureRequest &>(*req) = std::move(requested_textures_.front());
            requested_textures_.pop_front();
        }

        __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_read_file_str);
        OPTICK_EVENT("Read File");

        req->buf->set_data_len(0);
        req->mip_offset_to_init = 0xff;

        const Ren::StoragesRef &storages = ren_ctx_.storages();

        const auto &[img_main, img_cold] = storages.images[req->img];

        size_t read_offset = 0, read_size = 0;

        std::string path_buf = paths_.textures_path;
        path_buf += img_cold.name.c_str();

        bool read_success = true;

        if (img_cold.name.EndsWith(".dds") || img_cold.name.EndsWith(".DDS")) {
            if (req->orig_format == Ren::eFormat::Undefined) {
                Ren::DDSHeader header = {};

                size_t data_size = sizeof(Ren::DDSHeader);
                read_success = tex_reader_.ReadFileBlocking(path_buf.c_str(), 0 /* file_offset */,
                                                            sizeof(Ren::DDSHeader), &header, data_size);
                read_success &= (data_size == sizeof(Ren::DDSHeader));

                req->read_offset = sizeof(Ren::DDSHeader);

                if (read_success) {
                    Ren::ImgParams temp_params;
                    ParseDDSHeader(header, &temp_params);
                    req->orig_format = temp_params.format;
                    req->orig_w = temp_params.w;
                    req->orig_h = temp_params.h;
                    req->orig_mip_count = int(header.dwMipMapCount);

                    if (header.sPixelFormat.dwFourCC == Ren::FourCC_DX10) {
                        Ren::DDS_HEADER_DXT10 dx10_header = {};
                        tex_reader_.ReadFileBlocking(path_buf.c_str(), sizeof(Ren::DDSHeader),
                                                     sizeof(Ren::DDS_HEADER_DXT10), &dx10_header, data_size);

                        req->orig_format = Ren::FormatFromDXGIFormat(dx10_header.dxgiFormat);

                        read_offset += sizeof(Ren::DDS_HEADER_DXT10);
                    } else if (temp_params.format == Ren::eFormat::Undefined) {
                        // Try to use least significant bits of FourCC as format
                        const uint8_t val = (header.sPixelFormat.dwFourCC & 0xff);
                        if (val == 0x6f) {
                            req->orig_format = Ren::eFormat::R16F;
                        } else if (val == 0x70) {
                            req->orig_format = Ren::eFormat::RG16F;
                        } else if (val == 0x71) {
                            req->orig_format = Ren::eFormat::RGBA16F;
                        } else if (val == 0x72) {
                            req->orig_format = Ren::eFormat::R32F;
                        } else if (val == 0x73) {
                            req->orig_format = Ren::eFormat::RG32F;
                        } else if (val == 0x74) {
                            req->orig_format = Ren::eFormat::RGBA32F;
                        }
                    }
                }
            }
            read_offset += req->read_offset;
        } else if (img_cold.name.EndsWith(".ktx") || img_cold.name.EndsWith(".KTX")) {
            assert(false && "Not implemented!");
            read_offset += sizeof(Ren::KTXHeader);
        } else {
            read_success = false;
        }

        if (read_success) {
            const Ren::ImgParams &cur_p = img_cold.params;

            const int max_load_w = std::max(cur_p.w * (1 << mip_levels_per_request_), 256);
            const int max_load_h = std::max(cur_p.h * (1 << mip_levels_per_request_), 256);

            int w = int(req->orig_w), h = int(req->orig_h);
            for (int i = 0; i < req->orig_mip_count; i++) {
                const int data_len = Ren::GetDataLenBytes(w, h, req->orig_format);

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
            assert(img_cold.params.w == req->orig_w || img_cold.params.h != req->orig_h);

            if (read_size) {
                read_success =
                    tex_reader_.ReadFileNonBlocking(path_buf.c_str(), read_offset, read_size, *req->buf, req->ev);
                assert(req->buf->data_len() == read_size);
            }
        }

        { //
            std::lock_guard<std::mutex> _(tex_requests_lock_);
            req->state = read_success ? eRequestState::PendingIO : eRequestState::PendingError;
        }

        __itt_task_end(__g_itt_domain);
    }
}

void Eng::SceneManager::EstimateTextureMemory(const int portion_size) {
    OPTICK_EVENT();
    if (scene_data_.name_to_texture.empty()) {
        return;
    }

    const int BucketSize = 16;
    scene_data_.texture_mem_buckets.resize((scene_data_.name_to_texture.capacity() + BucketSize - 1) / BucketSize);

    uint64_t mem_after_estimation = scene_data_.estimated_texture_mem.load();

    const auto &images = ren_ctx_.storages().images;

    for (int i = 0; i < portion_size; i++) {
        scene_data_.tex_mem_bucket_index =
            (scene_data_.tex_mem_bucket_index + 1) % scene_data_.texture_mem_buckets.size();
        uint32_t &bucket = scene_data_.texture_mem_buckets[scene_data_.tex_mem_bucket_index];
        mem_after_estimation -= bucket;
        bucket = 0;

        const uint32_t start = scene_data_.tex_mem_bucket_index * BucketSize;
        const uint32_t end = std::min(start + BucketSize, uint32_t(scene_data_.name_to_texture.capacity()));

        uint32_t index = scene_data_.name_to_texture.FindOccupiedInRange(start, end);
        while (index != end) {
            const Ren::ImageHandle tex = scene_data_.name_to_texture.at(index).val;

            bucket += GetDataLenBytes(images[tex].second.params);

            index = scene_data_.name_to_texture.FindOccupiedInRange(index + 1, end);
        }

        mem_after_estimation += bucket;
    }

    if (scene_data_.estimated_texture_mem.exchange(mem_after_estimation) != mem_after_estimation) {
        ren_ctx_.log()->Info("Estimated tex memory is %.3f MB", double(mem_after_estimation) / (1024.0f * 1024.0f));

        std::lock_guard<std::mutex> _(tex_requests_lock_);
        tex_loader_cnd_.notify_one();
    }
}

bool Eng::SceneManager::ProcessPendingTextures(const int portion_size) {
    using namespace SceneManagerConstants;

    OPTICK_GPU_EVENT("ProcessPendingTextures");

    bool finished = true;

    auto &images = ren_ctx_.storages().images;

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
            finished &= requested_textures_.empty();
            for (int j = 0; j < int(io_pending_tex_.size()); j++) {
                finished &= io_pending_tex_[j].state == eRequestState::Idle;
                if (io_pending_tex_[j].state == eRequestState::PendingIO) {
                    res = io_pending_tex_[j].ev.GetResult(false /* block */, &bytes_read);
                    if (res != Sys::eFileReadResult::Pending) {
                        req = &io_pending_tex_[j];
                        break;
                    }
                } else if (io_pending_tex_[j].state == eRequestState::PendingUpdate) {
                    TextureRequestPending *_req = &io_pending_tex_[j];
                    auto *stage_buf = static_cast<TextureUpdateFileBuf *>(_req->buf.get());
                    const auto _res = stage_buf->fence.ClientWaitSync(0 /* timeout_us */);
                    if (_res == Ren::eWaitResult::Fail) {
                        ren_ctx_.log()->Error("Waiting on fence failed!");

                        static_cast<TextureRequest &>(*_req) = {};
                        _req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    } else if (_res != Ren::eWaitResult::Timeout) {
                        SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, _req->img, _req->img);

                        const Ren::ImageCold &img_cold = images[_req->img].second;
                        if (img_cold.params.w != _req->orig_w || img_cold.params.h != _req->orig_h) {
                            // process texture further (for next mip levels)
                            _req->sort_key = 0xffffffff;
                            requested_textures_.push_back(std::move(*_req));
                        } else {
                            std::lock_guard<std::mutex> _lock(gc_textures_mtx_);
                            finished_textures_.push_back(std::move(*_req));
                        }

                        static_cast<TextureRequest &>(*_req) = {};
                        _req->state = eRequestState::Idle;
                        tex_loader_cnd_.notify_one();
                    }
                } else if (io_pending_tex_[j].state == eRequestState::PendingError) {
                    TextureRequestPending *_req = &io_pending_tex_[j];
                    static_cast<TextureRequest &>(*_req) = {};
                    _req->state = eRequestState::Idle;
                    tex_loader_cnd_.notify_one();
                }
            }
        }

        if (req) {
            OPTICK_GPU_EVENT("Process pending texture");

            if (res == Sys::eFileReadResult::Successful) {
                assert(dynamic_cast<TextureUpdateFileBuf *>(req->buf.get()));
                auto *stage_buf = static_cast<TextureUpdateFileBuf *>(req->buf.get());

                const uint64_t t1_us = Sys::GetTimeUs();

                int w = std::max(int(req->orig_w) >> req->mip_offset_to_init, 1);
                int h = std::max(int(req->orig_h) >> req->mip_offset_to_init, 1);

                // stage_buf->fence.ClientWaitSync();
                ren_ctx_.BegSingleTimeCommands(stage_buf->cmd_buf);

                Ren::ImgParams p = images[req->img].second.params;
                const int new_mip_count =
                    (p.flags & Ren::eImgFlags::Stub) ? req->mip_count_to_init : (p.mip_count + req->mip_count_to_init);
                p.flags &= ~Ren::Bitmask(Ren::eImgFlags::Stub);
                images[req->img].second.params = p;

                Ren::ImgParams new_params = images[req->img].second.params;
                new_params.format = req->orig_format;
                new_params.w = w;
                new_params.h = h;
                new_params.mip_count = new_mip_count;

                const Ren::ImageHandle new_img = ren_ctx_.CreateImage(
                    req->img, new_params, scene_data_.persistent_data->mem_allocs.get(), stage_buf->cmd_buf);
                const auto &[img_main, img_cold] = images[new_img];

                int data_off = int(req->buf->data_off());
                for (int j = int(req->mip_offset_to_init); j < int(req->mip_offset_to_init) + req->mip_count_to_init;
                     j++) {
                    if (data_off >= int(bytes_read)) {
                        ren_ctx_.log()->Error("File %s has not enough data!", img_cold.name.c_str());
                        break;
                    }
                    const int data_len = GetDataLenBytes(w, h, req->orig_format);
                    const int mip_index = j - req->mip_offset_to_init;

                    Image_SetSubImage(ren_ctx_.api(), img_main, img_cold, 0, mip_index, Ren::Vec3i{0},
                                      Ren::Vec3i{w, h, 1}, req->orig_format, stage_buf->stage_buf(), stage_buf->cmd_buf,
                                      data_off, data_len);

                    data_off += data_len;
                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                }

                stage_buf->fence = ren_ctx_.EndSingleTimeCommands(stage_buf->cmd_buf);

                SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, req->img, new_img);
                ren_ctx_.ReleaseImage(req->img);
                req->img = new_img;

                scene_data_.name_to_texture.Set(img_cold.name, new_img);

                const uint64_t t2_us = Sys::GetTimeUs();

                ren_ctx_.log()->Info("Texture %s loaded (%.3f ms)", img_cold.name.c_str(),
                                     double(t2_us - t1_us) * 0.001);
            } else if (res == Sys::eFileReadResult::Failed) {
                ren_ctx_.log()->Error("Error loading %s", images[req->img].second.name.c_str());
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

    { // Process GC'ed textures
        std::lock_guard<std::mutex> lock(gc_textures_mtx_);
        OPTICK_GPU_EVENT("GC textures");
        for (int i = 0; i < portion_size && !gc_textures_.empty(); i++) {
            auto &req = gc_textures_.front();

            const auto &[img_main, img_cold] = images[req.img];
            ren_ctx_.log()->Warning("Texture %s is being garbage collected", img_cold.name.c_str());

            Ren::ImgParams new_params = img_cold.params;

            // drop to lowest lod
            new_params.w = std::max(new_params.w >> new_params.mip_count, 1);
            new_params.h = std::max(new_params.h >> new_params.mip_count, 1);
            new_params.mip_count = 1;

            const Ren::ImageHandle new_img = ren_ctx_.CreateImage(
                req.img, new_params, scene_data_.persistent_data->mem_allocs.get(), ren_ctx_.current_cmd_buf());

            SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, req.img, new_img);
            ren_ctx_.ReleaseImage(req.img);
            req.img = new_img;

            { // send texture for processing
                std::lock_guard<std::mutex> _lock(tex_requests_lock_);
                requested_textures_.push_back(req);
            }

            gc_textures_.pop_front();
        }
    }

    finished |= (scene_data_.estimated_texture_mem.load() > tex_memory_limit_.load());

    return finished;
}

void Eng::SceneManager::RebuildMaterialTextureGraph() {
    OPTICK_EVENT();

    const Ren::StoragesRef &storages = ren_ctx_.storages();

    // reset texture user
    for (const auto &texture : scene_data_.name_to_texture) {
        Ren::ImageCold &img_cold = storages.images[texture.val].second;
        img_cold.first_user = 0xffffffff;
    }
    // assign material index as the first user
    for (auto it = scene_data_.name_to_material.begin(); it != scene_data_.name_to_material.end(); ++it) {
        const auto &[mat_main, mat_cold] = storages.materials[it->val];

        mat_cold.next_texture_user = {};
        mat_cold.next_texture_user.resize(mat_main.textures.size(), 0xffffffff);

        for (int i = 0; i < int(mat_main.textures.size()); ++i) {
            const Ren::ImageHandle tex = mat_main.textures[i];

            const auto &[img_main, img_cold] = storages.images[tex];
            if (img_cold.first_user == 0xffffffff) {
                img_cold.first_user = it->val.index;
            } else if (img_cold.first_user != it->val.index) {
                mat_cold.next_texture_user[i] = img_cold.first_user;
                for (int j = i + 1; j < int(mat_main.textures.size()); ++j) {
                    if (mat_main.textures[j] == tex) {
                        mat_cold.next_texture_user[j] = img_cold.first_user;
                    }
                }
                img_cold.first_user = it->val.index;
            }
        }
    }
}

void Eng::SceneManager::UpdateTexturePriorities(const Ren::Span<const TexEntry> visible_textures,
                                                const Ren::Span<const TexEntry> desired_textures) {
    OPTICK_EVENT();

    TexturesGCIteration(visible_textures, desired_textures);

    { // Update requested textures
        std::unique_lock<std::mutex> lock(tex_requests_lock_);

        bool kick_loader_thread = false;
        for (auto it = requested_textures_.begin(); it != requested_textures_.end(); ++it) {
            const TexEntry *found_entry = nullptr;

            { // search among visible textures first
                const TexEntry *beg = visible_textures.begin();
                const TexEntry *end = visible_textures.end();

                const TexEntry *entry = std::lower_bound(
                    beg, end, it->img.index, [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });
                if (entry != end && entry->index == it->img.index) {
                    found_entry = entry;
                }
            }

            if (!found_entry) { // search among surrounding textures
                const TexEntry *beg = desired_textures.begin();
                const TexEntry *end = desired_textures.end();

                const TexEntry *entry = std::lower_bound(
                    beg, end, it->img.index, [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });
                if (entry != end && entry->index == it->img.index) {
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

void Eng::SceneManager::TexturesGCIteration(const Ren::Span<const TexEntry> visible_textures,
                                            const Ren::Span<const TexEntry> desired_textures) {
    using namespace SceneManagerConstants;

    OPTICK_EVENT();

    const int FinishedPortion = 16;

    const bool enable_gc = (scene_data_.estimated_texture_mem.load() >= 9 * tex_memory_limit_.load() / 10);

    std::lock_guard<std::mutex> _lock(gc_textures_mtx_);

    auto start_it = finished_textures_.begin() + std::min(finished_index_, uint32_t(finished_textures_.size()));
    finished_index_ += FinishedPortion;

    if (finished_index_ >= finished_textures_.size()) {
        finished_index_ = 0;
    }

    int processed_count = 0;
    for (auto it = start_it; it != finished_textures_.end();) {
        const TexEntry *found_entry = nullptr;

        { // search among visible textures first
            const TexEntry *beg = visible_textures.begin();
            const TexEntry *end = visible_textures.end();

            const TexEntry *entry = std::lower_bound(
                beg, end, it->img.index, [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });
            if (entry != end && entry->index == it->img.index) {
                found_entry = entry;
            }
        }

        if (!found_entry) { // search among surrounding textures
            const TexEntry *beg = desired_textures.begin();
            const TexEntry *end = desired_textures.end();

            const TexEntry *entry = std::lower_bound(
                beg, end, it->img.index, [](const TexEntry &t1, const uint32_t t2) { return t1.index < t2; });
            if (entry != end && entry->index == it->img.index) {
                found_entry = entry;
            }
        }

        it->frame_dist += std::max(int(finished_textures_.size()) / FinishedPortion, 1);

        const Ren::ImageCold &img_cold = ren_ctx_.storages().images[it->img].second;
        if (found_entry) {
            it->sort_key = found_entry->sort_key;
            it->frame_dist = 0;

            ++it;
        } else if (it->frame_dist > 1000 && enable_gc && img_cold.params.w > 16 && img_cold.params.w == it->orig_w &&
                   img_cold.params.h > 16 && img_cold.params.h == it->orig_h) {
            // Reduce texture's mips
            it->frame_dist = 0;
            it->sort_key = 0xffffffff;

            gc_textures_.push_back(*it);

            it = finished_textures_.erase(it);
        } else {
            ++it;
        }

        if (++processed_count >= FinishedPortion) {
            break;
        }
    }
}

void Eng::SceneManager::InvalidateTexture(const Ren::ImageHandle handle) {
    SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, handle, handle);
}

void Eng::SceneManager::StartTextureLoaderThread(const int requests_count, const int mip_levels_per_request) {
    for (int i = 0; i < requests_count; i++) {
        TextureRequestPending &req = io_pending_tex_.emplace_back();
        req.buf = std::make_unique<TextureUpdateFileBuf>(&ren_ctx_.api(), ren_ctx_.log());
    }
    mip_levels_per_request_ = mip_levels_per_request;
    tex_loader_stop_ = false;
    tex_loader_thread_ = std::thread(&SceneManager::TextureLoaderProc, this);
}

void Eng::SceneManager::StopTextureLoaderThread() {
    if (!tex_loader_stop_) {
        { // stop texture loading thread
            std::unique_lock<std::mutex> lock(tex_requests_lock_);
            tex_loader_stop_ = true;
            tex_loader_cnd_.notify_one();
        }
        assert(tex_loader_thread_.joinable());
        tex_loader_thread_.join();
    }
    for (int i = 0; i < int(io_pending_tex_.size()); ++i) {
        size_t bytes_read = 0;
        io_pending_tex_[i].ev.GetResult(true /* block */, &bytes_read);
        io_pending_tex_[i].state = eRequestState::Idle;
        io_pending_tex_[i].img = {};
    }
    io_pending_tex_.clear();
    requested_textures_.clear();
    {
        std::unique_lock<std::mutex> lock(gc_textures_mtx_);
        finished_textures_.clear();
        gc_textures_.clear();
    }
}

void Eng::SceneManager::ForceTextureReload() {
    StopTextureLoaderThread();

    const auto &images = ren_ctx_.storages().images;

    std::vector<Ren::TransitionInfo> img_transitions;
    img_transitions.reserve(scene_data_.name_to_texture.size());

    // Reset textures to 1x1 mip and send to processing
    for (auto it = std::begin(scene_data_.name_to_texture); it != std::end(scene_data_.name_to_texture); ++it) {
        Ren::ImgParams new_params;
        { // Get params
            const auto &[img_main, img_cold] = images[it->val];

            new_params = img_cold.params;
            new_params.flags |= Ren::eImgFlags::Stub;

            // drop to lowest lod
            new_params.w = std::max(new_params.w >> (new_params.mip_count - 1), 1);
            new_params.h = std::max(new_params.h >> (new_params.mip_count - 1), 1);
            new_params.mip_count = 1;

            if (new_params.w == img_cold.params.w && new_params.h == img_cold.params.h) {
                // Already has the lowest mip loaded
                continue;
            }
        }

        const Ren::ImageHandle new_img = ren_ctx_.CreateImage(
            it->val, new_params, scene_data_.persistent_data->mem_allocs.get(), ren_ctx_.current_cmd_buf());

        img_transitions.emplace_back(new_img, Ren::eResState::ShaderResource);

        SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, it->val, new_img);
        ren_ctx_.ReleaseImage(it->val);
        it->val = new_img;

        TextureRequest req;
        req.img = new_img;
        requested_textures_.push_back(std::move(req));
    }

    if (!img_transitions.empty()) {
        TransitionResourceStates(ren_ctx_.api(), ren_ctx_.storages(), ren_ctx_.current_cmd_buf(), Ren::AllStages,
                                 Ren::AllStages, img_transitions);
    }

    fill(begin(scene_data_.texture_mem_buckets), end(scene_data_.texture_mem_buckets), 0);
    scene_data_.tex_mem_bucket_index = 0;
    scene_data_.estimated_texture_mem = 0;

    StartTextureLoaderThread();
}

void Eng::SceneManager::ReleaseImages(const bool immediately) {
    StopTextureLoaderThread();
    assert(immediately);

    auto new_alloc = std::make_unique<Ren::MemAllocators>(
        "Scene Mem Allocs", &ren_ctx_.api(), 16 * 1024 * 1024 /* initial_block_size */, 1.5f /* growth_factor */,
        128 * 1024 * 1024 /* max_pool_size */);

    const auto &images = ren_ctx_.storages().images;

    std::vector<Ren::TransitionInfo> img_transitions;
    img_transitions.reserve(scene_data_.name_to_texture.size());

    // Reset textures to 1x1
    for (auto it = std::begin(scene_data_.name_to_texture); it != std::end(scene_data_.name_to_texture); ++it) {
        Ren::ImgParams p;
        Ren::String name_str;
        { // Get params
            const auto &[img_main, img_cold] = images[it->val];

            p = img_cold.params;
            p.format = Ren::eFormat::RGBA8;
            p.flags = Ren::eImgFlags::Stub;
            p.w = p.h = 1;
            p.mip_count = 1;

            name_str = img_cold.name;
        }

        // Initialize with fallback color
        // TODO: Use actual fallback color
        const uint8_t fallback_color[4] = {};
        const Ren::ImageHandle new_img = ren_ctx_.CreateImage(name_str, fallback_color, p, new_alloc.get());
        img_transitions.emplace_back(new_img, Ren::eResState::ShaderResource);

        SceneManagerInternal::CaptureMaterialTextureChange(ren_ctx_, scene_data_, it->val, new_img);
        ren_ctx_.ReleaseImage(it->val, true /* immediately */);
        it->val = new_img;

        TextureRequest req;
        req.img = new_img;
        requested_textures_.push_back(std::move(req));
    }

    if (!img_transitions.empty()) {
        Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();
        TransitionResourceStates(ren_ctx_.api(), ren_ctx_.storages(), cmd_buf, Ren::AllStages, Ren::AllStages,
                                 img_transitions);
        ren_ctx_.EndTempSingleTimeCommands(cmd_buf);
    }

    scene_data_.persistent_data->mem_allocs = std::move(new_alloc);

    fill(begin(scene_data_.texture_mem_buckets), end(scene_data_.texture_mem_buckets), 0);
    scene_data_.tex_mem_bucket_index = 0;
    scene_data_.estimated_texture_mem = 0;
}

void Eng::SceneManager::Release_BLASes(const bool immediately) {
    for (const Ren::AccStructHandle acc : scene_data_.persistent_data->rt_blases) {
        ren_ctx_.ReleaseAccStruct(acc, immediately);
    }
    scene_data_.persistent_data->rt_blases.clear();
}