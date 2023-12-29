#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/Time_.h>

#include "../utils/ShaderLoader.h"
#include "../Random.h"
#include "Renderer_Names.h"

#include "shaders/blit_bilateral_interface.h"
#include "shaders/blit_down_depth_interface.h"
#include "shaders/blit_down_interface.h"
#include "shaders/blit_gauss_interface.h"
#include "shaders/blit_ssao_interface.h"
#include "shaders/blit_static_vel_interface.h"
#include "shaders/blit_taa_interface.h"
#include "shaders/blit_upscale_interface.h"
#include "shaders/debug_velocity_interface.h"
#include "shaders/gbuffer_shade_interface.h"

namespace RendererInternal {
	extern const int TaaSampleCountStatic;
}

void Eng::Renderer::InitPipelines() {
	{ // Init skinning pipeline
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "skinning_prog", "internal/skinning.comp.glsl");
		assert(prog->ready());

		if (!pi_skinning_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Init gbuffer shading pipeline
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gbuffer_shade", "internal/gbuffer_shade.comp.glsl");
		assert(prog->ready());

		if (!pi_gbuf_shade_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}

		Ren::ProgramRef prog_hq = sh_.LoadProgram(ctx_, "gbuffer_shade_hq", "internal/gbuffer_shade.comp.glsl@HQ_HDR");
		assert(prog_hq->ready());

		if (!pi_gbuf_shade_hq_.Init(ctx_.api_ctx(), std::move(prog_hq), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Quad classification for SSR
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_classify_tiles", "internal/ssr_classify_tiles.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_classify_tiles_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Indirect dispatch arguments preparation for SSR
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "ssr_write_indirect_args", "internal/ssr_write_indirect_args.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_write_indirect_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // HQ screen-space ray tracing
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_trace_hq", "internal/ssr_trace_hq.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_trace_hq_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // RT dispatch arguments preparation for reflections
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "ssr_write_indir_rt_dispatch", "internal/ssr_write_indir_rt_dispatch.comp.glsl");
		assert(prog->ready());

		if (!pi_rt_write_indirect_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Reflections reprojection
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_reproject", "internal/ssr_reproject.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_reproject_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Reflections prefilter
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_prefilter", "internal/ssr_prefilter.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_prefilter_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Reflections accumulation
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_resolve_temporal", "internal/ssr_resolve_temporal.comp.glsl");
		assert(prog->ready());

		if (!pi_ssr_resolve_temporal_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Flat normals reconstruction
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "reconstruct_normals", "internal/reconstruct_normals.comp.glsl");
		assert(prog->ready());

		if (!pi_reconstruct_normals_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	blit_static_vel_prog_ = sh_.LoadProgram(ctx_, "blit_static_vel_prog", "internal/blit_static_vel.vert.glsl",
		"internal/blit_static_vel.frag.glsl");
	assert(blit_static_vel_prog_->ready());

	blit_gauss2_prog_ =
		sh_.LoadProgram(ctx_, "blit_gauss2", "internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
	assert(blit_gauss2_prog_->ready());

	blit_ao_prog_ = sh_.LoadProgram(ctx_, "blit_ao", "internal/blit_ssao.vert.glsl", "internal/blit_ssao.frag.glsl");
	assert(blit_ao_prog_->ready());

	blit_bilateral_prog_ = sh_.LoadProgram(ctx_, "blit_bilateral2", "internal/blit_bilateral.vert.glsl",
		"internal/blit_bilateral.frag.glsl");
	assert(blit_bilateral_prog_->ready());

	blit_taa_prog_ = sh_.LoadProgram(ctx_, "blit_taa_prog", "internal/blit_taa.vert.glsl",
		"internal/blit_taa.frag.glsl@USE_CLIPPING;USE_TONEMAP");
	assert(blit_taa_prog_->ready());

	blit_taa_static_prog_ = sh_.LoadProgram(ctx_, "blit_taa_static_prog", "internal/blit_taa.vert.glsl",
		"internal/blit_taa.frag.glsl@USE_STATIC_ACCUMULATION");
	assert(blit_taa_static_prog_->ready());

	blit_ssr_prog_ = sh_.LoadProgram(ctx_, "blit_ssr", "internal/blit_ssr.vert.glsl", "internal/blit_ssr.frag.glsl");
	assert(blit_ssr_prog_->ready());

	blit_ssr_dilate_prog_ = sh_.LoadProgram(ctx_, "blit_ssr_dilate", "internal/blit_ssr_dilate.vert.glsl",
		"internal/blit_ssr_dilate.frag.glsl");
	assert(blit_ssr_dilate_prog_->ready());

	blit_upscale_prog_ =
		sh_.LoadProgram(ctx_, "blit_upscale", "internal/blit_upscale.vert.glsl", "internal/blit_upscale.frag.glsl");
	assert(blit_upscale_prog_->ready());

	blit_down2_prog_ =
		sh_.LoadProgram(ctx_, "blit_down2", "internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
	assert(blit_down2_prog_->ready());

	blit_down_depth_prog_ = sh_.LoadProgram(ctx_, "blit_down_depth", "internal/blit_down_depth.vert.glsl",
		"internal/blit_down_depth.frag.glsl");
	assert(blit_down_depth_prog_->ready());

	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{ // Quad classification for GI
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_classify_tiles", "internal/gi_classify_tiles.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_classify_tiles_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Indirect dispatch arguments preparation for GI
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "gi_write_indirect_args", "internal/gi_write_indirect_args.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_write_indirect_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI screen-space tracing
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_trace_ss", "internal/gi_trace_ss.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_trace_ss_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI RT dispatch arguments preparation
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "gi_write_indir_rt_dispatch", "internal/gi_write_indir_rt_dispatch.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_rt_write_indirect_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI reprojection
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_reproject", "internal/gi_reproject.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_reproject_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI prefilter
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_prefilter", "internal/gi_prefilter.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_prefilter_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI accumulation
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_resolve_temporal", "internal/gi_resolve_temporal.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_resolve_temporal_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI blur
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "gi_blur", "internal/gi_blur.comp.glsl");
		assert(prog->ready());

		if (!pi_gi_blur_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // GI post-blur
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "gi_post_blur", "internal/gi_blur.comp.glsl@PER_PIXEL_KERNEL_ROTATION");
		assert(prog->ready());

		if (!pi_gi_post_blur_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow classify
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "sun_rt_sh_classify", "internal/rt_shadow_classify.comp.glsl");
		assert(prog->ready());

		if (!pi_shadow_classify_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun shadows
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "sun_shadows", "internal/sun_shadows.comp.glsl");
		assert(prog->ready());

		if (!pi_sun_shadows_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Prepare sun shadow mask
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "rt_shadow_prepare_mask", "internal/rt_shadow_prepare_mask.comp.glsl");
		assert(prog->ready());

		if (!pi_shadow_prepare_mask_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow classify tiles
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "rt_shadow_classify_tiles", "internal/rt_shadow_classify_tiles.comp.glsl");
		assert(prog->ready());

		if (!pi_shadow_classify_tiles_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow filter 0
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "rt_shadow_filter_0", "internal/rt_shadow_filter.comp.glsl@PASS_0");
		assert(prog->ready());

		if (!pi_shadow_filter_[0].Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow filter 1
		Ren::ProgramRef prog =
			sh_.LoadProgram(ctx_, "rt_shadow_filter_1", "internal/rt_shadow_filter.comp.glsl@PASS_1");
		assert(prog->ready());

		if (!pi_shadow_filter_[1].Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow filter 2
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "rt_shadow_filter_2", "internal/rt_shadow_filter.comp.glsl");
		assert(prog->ready());

		if (!pi_shadow_filter_[2].Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
	{ // Sun RT Shadow debug
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "rt_shadow_debug", "internal/rt_shadow_debug.comp.glsl");
		assert(prog->ready());

		if (!pi_shadow_debug_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	{ // Velocity debugging
		Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "debug_velocity", "internal/debug_velocity.comp.glsl");
		assert(prog->ready());

		if (!pi_debug_velocity_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
			ctx_.log()->Error("Renderer: failed to initialize pipeline!");
		}
	}
}

void Eng::Renderer::AddBuffersUpdatePass(CommonBuffers& common_buffers) {
	auto& update_bufs = rp_builder_.AddPass("UPDATE BUFFERS");

	{ // create skin transforms buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = SkinTransformsBufChunkSize;
		common_buffers.skin_transforms_res = update_bufs.AddTransferOutput(SKIN_TRANSFORMS_BUF, desc);
	}
	{ // create shape keys buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = ShapeKeysBufChunkSize;
		common_buffers.shape_keys_res = update_bufs.AddTransferOutput(SHAPE_KEYS_BUF, desc);
	}
	{ // create instance indices buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = InstanceIndicesBufChunkSize;
		common_buffers.instance_indices_res = update_bufs.AddTransferOutput(INSTANCE_INDICES_BUF, desc);
	}
	{ // create uniform buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Uniform;
		desc.size = SharedDataBlockSize;
		common_buffers.shared_data_res = update_bufs.AddTransferOutput(SHARED_DATA_BUF, desc);
	}
	{ // create atomic counter buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Storage;
		desc.size = sizeof(uint32_t);
		common_buffers.atomic_cnt_res = update_bufs.AddTransferOutput(ATOMIC_CNT_BUF, desc);
	}

	update_bufs.set_execute_cb([this, &common_buffers](RpBuilder& builder) {
		Ren::Context& ctx = builder.ctx();
		RpAllocBuf& skin_transforms_buf = builder.GetWriteBuffer(common_buffers.skin_transforms_res);
		RpAllocBuf& shape_keys_buf = builder.GetWriteBuffer(common_buffers.shape_keys_res);
		// RpAllocBuf &instances_buf = builder.GetWriteBuffer(common_buffers.instances_res);
		RpAllocBuf& instance_indices_buf = builder.GetWriteBuffer(common_buffers.instance_indices_res);
		RpAllocBuf& shared_data_buf = builder.GetWriteBuffer(common_buffers.shared_data_res);
		RpAllocBuf& atomic_cnt_buf = builder.GetWriteBuffer(common_buffers.atomic_cnt_res);

		Ren::UpdateBuffer(*skin_transforms_buf.ref, 0, p_list_->skin_transforms.count * sizeof(SkinTransform),
			p_list_->skin_transforms.data, *p_list_->skin_transforms_stage_buf,
			ctx.backend_frame() * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize,
			ctx.current_cmd_buf());

		Ren::UpdateBuffer(*shape_keys_buf.ref, 0, p_list_->shape_keys_data.count * sizeof(ShapeKeyData),
			p_list_->shape_keys_data.data, *p_list_->shape_keys_stage_buf,
			ctx.backend_frame() * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize, ctx.current_cmd_buf());

		if (!instance_indices_buf.tbos[0]) {
			instance_indices_buf.tbos[0] =
				ctx.CreateTexture1D("Instance Indices TBO", instance_indices_buf.ref, Ren::eTexFormat::RawRG32UI, 0,
					InstanceIndicesBufChunkSize);
		}

		Ren::UpdateBuffer(*instance_indices_buf.ref, 0, p_list_->instance_indices.count * sizeof(Ren::Vec2i),
			p_list_->instance_indices.data, *p_list_->instance_indices_stage_buf,
			ctx.backend_frame() * InstanceIndicesBufChunkSize, InstanceIndicesBufChunkSize,
			ctx.current_cmd_buf());

		{ // Prepare data that is shared for all instances
			SharedDataBlock shrd_data;

			shrd_data.view_matrix = p_list_->draw_cam.view_matrix();
			shrd_data.proj_matrix = p_list_->draw_cam.proj_matrix();

			shrd_data.uTaaInfo[0] = p_list_->draw_cam.px_offset()[0];
#if defined(USE_VK_RENDER)
			shrd_data.uTaaInfo[1] = -p_list_->draw_cam.px_offset()[1];
#else
			shrd_data.uTaaInfo[1] = p_list_->draw_cam.px_offset()[1];
#endif
			shrd_data.uTaaInfo[2] = reinterpret_cast<const float&>(view_state_.frame_index);
			shrd_data.uTaaInfo[3] = std::tan(0.5f * p_list_->draw_cam.angle() * Ren::Pi<float>() / 180.0f);

			{ // Ray Tracing Gems II, Listing 49-1
				const Ren::Plane& l = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Left);
				const Ren::Plane& r = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Right);
				const Ren::Plane& b = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Bottom);
				const Ren::Plane& t = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Top);

				const float x0 = l.n[2] / l.n[0];
				const float x1 = r.n[2] / r.n[0];
				const float y0 = b.n[2] / b.n[1];
				const float y1 = t.n[2] / t.n[1];

				// View space position from screen space uv [0, 1]
				//  ray.xy = (uFrustumInfo.zw * uv + uFrustumInfo.xy) * mix(zDistanceNeg, -1.0, bIsOrtho)
				//  ray.z = 1.0 * zDistanceNeg

				shrd_data.uFrustumInfo[0] = -x0;
				shrd_data.uFrustumInfo[1] = -y0;
				shrd_data.uFrustumInfo[2] = x0 - x1;
				shrd_data.uFrustumInfo[3] = y0 - y1;

				view_state_.frustum_info = shrd_data.uFrustumInfo;

				auto ReconstructViewPosition = [](const Ren::Vec2f uv, const Ren::Vec4f& cam_frustum,
					const float view_z, const float is_ortho) {
						Ren::Vec3f p;
						p[0] = uv[0] * cam_frustum[2] + cam_frustum[0];
						p[1] = uv[1] * cam_frustum[3] + cam_frustum[1];

						p[0] *= view_z * (1.0f - std::abs(is_ortho)) + is_ortho;
						p[1] *= view_z * (1.0f - std::abs(is_ortho)) + is_ortho;
						p[2] = view_z;

						return p;
					};
			}

			shrd_data.proj_matrix[2][0] += p_list_->draw_cam.px_offset()[0];
			shrd_data.proj_matrix[2][1] += p_list_->draw_cam.px_offset()[1];

			Ren::Mat4f view_matrix_no_translation = shrd_data.view_matrix;
			view_matrix_no_translation[3][0] = view_matrix_no_translation[3][1] = view_matrix_no_translation[3][2] = 0;

			shrd_data.view_proj_no_translation = shrd_data.proj_matrix * view_matrix_no_translation;
			shrd_data.prev_view_proj_no_translation = view_state_.prev_clip_from_world_no_translation;
			shrd_data.inv_view_matrix = Inverse(shrd_data.view_matrix);
			shrd_data.inv_proj_matrix = Inverse(shrd_data.proj_matrix);
			shrd_data.inv_view_proj_no_translation = Inverse(shrd_data.view_proj_no_translation);
			// delta matrix between current and previous frame
			shrd_data.delta_matrix =
				view_state_.prev_clip_from_view * (view_state_.down_buf_view_from_world * shrd_data.inv_view_matrix);

			if (p_list_->shadow_regions.count) {
				assert(p_list_->shadow_regions.count <= REN_MAX_SHADOWMAPS_TOTAL);
				memcpy(&shrd_data.shadowmap_regions[0], &p_list_->shadow_regions.data[0],
					sizeof(ShadowMapRegion) * p_list_->shadow_regions.count);
			}

			shrd_data.sun_dir =
				Ren::Vec4f{ p_list_->env.sun_dir[0], p_list_->env.sun_dir[1], p_list_->env.sun_dir[2], 0.0f };
			shrd_data.sun_col =
				Ren::Vec4f{ p_list_->env.sun_col[0], p_list_->env.sun_col[1], p_list_->env.sun_col[2], 0.0f };

			// actual resolution and full resolution
			shrd_data.res_and_fres = Ren::Vec4f{ float(view_state_.act_res[0]), float(view_state_.act_res[1]),
												float(view_state_.scr_res[0]), float(view_state_.scr_res[1]) };

			const float near = p_list_->draw_cam.near(), far = p_list_->draw_cam.far();
			const float time_s = 0.001f * Sys::GetTimeMs();
			const float transparent_near = near;
			const float transparent_far = 16.0f;
			const int transparent_mode =
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
				(render_flags_ & EnableOIT) ? 2 : 0;
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
				(render_flags_ & EnableOIT) ? 1 : 0;
#else
				0;
#endif

			shrd_data.transp_params_and_time =
				Ren::Vec4f{ std::log(transparent_near), std::log(transparent_far) - std::log(transparent_near),
						   float(transparent_mode), time_s };
			shrd_data.clip_info = Ren::Vec4f{ near * far, near, far, std::log2(1.0f + far / near) };
			view_state_.clip_info = shrd_data.clip_info;

			{ // rotator for GI prefilter
				const float rand_angle = rand_.GetNormalizedFloat() * 2.0f * Ren::Pi<float>();
				const float ca = std::cos(rand_angle);
				const float sa = std::sin(rand_angle);
				view_state_.rand_rotators[0] = Ren::Vec4f{ ca, sa, -sa, ca };
			}
			{ // 2 rotators for GI blur (perpendicular to each other)
				const float rand_angle = rand_.GetNormalizedFloat() * 2.0f * Ren::Pi<float>();
				const float ca = std::cos(rand_angle);
				const float sa = std::sin(rand_angle);
				view_state_.rand_rotators[1] = Ren::Vec4f{ -sa, ca, -ca, sa };
				view_state_.rand_rotators[2] = Ren::Vec4f{ ca, sa, -sa, ca };
			}

			const Ren::Vec3f& cam_pos = p_list_->draw_cam.world_position();
			shrd_data.prev_cam_pos =
				Ren::Vec4f{ view_state_.prev_cam_pos[0], view_state_.prev_cam_pos[1], view_state_.prev_cam_pos[2], 0.0f };
			shrd_data.cam_pos_and_gamma = Ren::Vec4f{ cam_pos[0], cam_pos[1], cam_pos[2], 2.2f };
			shrd_data.wind_scroll =
				Ren::Vec4f{ p_list_->env.curr_wind_scroll_lf[0], p_list_->env.curr_wind_scroll_lf[1],
						   p_list_->env.curr_wind_scroll_hf[0], p_list_->env.curr_wind_scroll_hf[1] };
			shrd_data.wind_scroll_prev =
				Ren::Vec4f{ p_list_->env.prev_wind_scroll_lf[0], p_list_->env.prev_wind_scroll_lf[1],
						   p_list_->env.prev_wind_scroll_hf[0], p_list_->env.prev_wind_scroll_hf[1] };

			memcpy(&shrd_data.probes[0], p_list_->probes.data, sizeof(ProbeItem) * p_list_->probes.count);
			memcpy(&shrd_data.ellipsoids[0], p_list_->ellipsoids.data, sizeof(EllipsItem) * p_list_->ellipsoids.count);

			Ren::UpdateBuffer(*shared_data_buf.ref, 0, sizeof(SharedDataBlock), &shrd_data,
				*p_list_->shared_data_stage_buf, ctx.backend_frame() * SharedDataBlockSize,
				SharedDataBlockSize, ctx.current_cmd_buf());
		}

		atomic_cnt_buf.ref->Fill(0, sizeof(uint32_t), 0, ctx.current_cmd_buf());
		});
}

void Eng::Renderer::AddLightBuffersUpdatePass(CommonBuffers& common_buffers) {
	auto& update_light_bufs = rp_builder_.AddPass("UPDATE LBUFFERS");

	{ // create cells buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = CellsBufChunkSize;
		common_buffers.cells_res = update_light_bufs.AddTransferOutput(CELLS_BUF, desc);
	}
	{ // create lights buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = LightsBufChunkSize;
		common_buffers.lights_res = update_light_bufs.AddTransferOutput(LIGHTS_BUF, desc);
	}
	{ // create decals buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = DecalsBufChunkSize;
		common_buffers.decals_res = update_light_bufs.AddTransferOutput(DECALS_BUF, desc);
	}
	{ // create items buffer
		RpBufDesc desc;
		desc.type = Ren::eBufType::Texture;
		desc.size = ItemsBufChunkSize;
		common_buffers.items_res = update_light_bufs.AddTransferOutput(ITEMS_BUF, desc);
	}

	update_light_bufs.set_execute_cb([this, &common_buffers](RpBuilder& builder) {
		Ren::Context& ctx = builder.ctx();
		RpAllocBuf& cells_buf = builder.GetWriteBuffer(common_buffers.cells_res);
		RpAllocBuf& lights_buf = builder.GetWriteBuffer(common_buffers.lights_res);
		RpAllocBuf& decals_buf = builder.GetWriteBuffer(common_buffers.decals_res);
		RpAllocBuf& items_buf = builder.GetWriteBuffer(common_buffers.items_res);

		if (!cells_buf.tbos[0]) {
			cells_buf.tbos[0] =
				ctx.CreateTexture1D("Cells TBO", cells_buf.ref, Ren::eTexFormat::RawRG32UI, 0, CellsBufChunkSize);
		}

		Ren::UpdateBuffer(*cells_buf.ref, 0, p_list_->cells.count * sizeof(CellData), p_list_->cells.data,
			*p_list_->cells_stage_buf, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize,
			ctx.current_cmd_buf());

		if (!lights_buf.tbos[0]) {
			lights_buf.tbos[0] =
				ctx.CreateTexture1D("Lights TBO", lights_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, LightsBufChunkSize);
		}

		Ren::UpdateBuffer(*lights_buf.ref, 0, p_list_->lights.count * sizeof(LightItem), p_list_->lights.data,
			*p_list_->lights_stage_buf, ctx.backend_frame() * LightsBufChunkSize, LightsBufChunkSize,
			ctx.current_cmd_buf());

		if (!decals_buf.tbos[0]) {
			decals_buf.tbos[0] =
				ctx.CreateTexture1D("Decals TBO", decals_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, DecalsBufChunkSize);
		}

		Ren::UpdateBuffer(*decals_buf.ref, 0, p_list_->decals.count * sizeof(DecalItem), p_list_->decals.data,
			*p_list_->decals_stage_buf, ctx.backend_frame() * DecalsBufChunkSize, DecalsBufChunkSize,
			ctx.current_cmd_buf());

		if (!items_buf.tbos[0]) {
			items_buf.tbos[0] =
				ctx.CreateTexture1D("Items TBO", items_buf.ref, Ren::eTexFormat::RawRG32UI, 0, ItemsBufChunkSize);
		}

		if (p_list_->items.count) {
			Ren::UpdateBuffer(*items_buf.ref, 0, p_list_->items.count * sizeof(ItemData), p_list_->items.data,
				*p_list_->items_stage_buf, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize,
				ctx.current_cmd_buf());
		}
		else {
			const ItemData dummy = {};
			Ren::UpdateBuffer(*items_buf.ref, 0, sizeof(ItemData), &dummy, *p_list_->items_stage_buf,
				ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
		}
		});
}

void Eng::Renderer::AddSkydomePass(const CommonBuffers& common_buffers, const bool clear,
	FrameTextures& frame_textures) {
	if (p_list_->env.env_map) {
		auto& skymap = rp_builder_.AddPass("SKYDOME");
		RpResRef shared_data_buf = skymap.AddUniformBufferInput(
			common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
		RpResRef env_tex = skymap.AddTextureInput(p_list_->env.env_map, Ren::eStageBits::FragmentShader);
		RpResRef vtx_buf1 = skymap.AddVertexBufferInput(ctx_.default_vertex_buf1());
		RpResRef vtx_buf2 = skymap.AddVertexBufferInput(ctx_.default_vertex_buf2());
		RpResRef ndx_buf = skymap.AddIndexBufferInput(ctx_.default_indices_buf());

		frame_textures.color = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
		frame_textures.specular = skymap.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
		frame_textures.depth = skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

		rp_skydome_.Setup(*p_list_, &view_state_, clear, vtx_buf1, vtx_buf2, ndx_buf, shared_data_buf, env_tex,
			frame_textures.color, frame_textures.specular, frame_textures.depth);
		skymap.set_executor(&rp_skydome_);
	}
	else {
		// TODO: Physical sky
	}
}

void Eng::Renderer::AddGBufferFillPass(const CommonBuffers& common_buffers, const PersistentGpuData& persistent_data,
	const BindlessTextureData& bindless, FrameTextures& frame_textures) {
	using Stg = Ren::eStageBits;

	auto& gbuf_fill = rp_builder_.AddPass("GBUFFER FILL");
	const RpResRef vtx_buf1 = gbuf_fill.AddVertexBufferInput(ctx_.default_vertex_buf1());
	const RpResRef vtx_buf2 = gbuf_fill.AddVertexBufferInput(ctx_.default_vertex_buf2());
	const RpResRef ndx_buf = gbuf_fill.AddIndexBufferInput(ctx_.default_indices_buf());

	const RpResRef materials_buf = gbuf_fill.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(USE_GL_RENDER)
	const RpResRef textures_buf = gbuf_fill.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
	const RpResRef textures_buf = {};
#endif

	const RpResRef noise_tex = gbuf_fill.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
	const RpResRef dummy_black = gbuf_fill.AddTextureInput(dummy_black_, Stg::FragmentShader);

	const RpResRef instances_buf = gbuf_fill.AddStorageReadonlyInput(
		persistent_data.instance_buf, persistent_data.instance_buf_tbo, Stg::VertexShader);
	const RpResRef instances_indices_buf =
		gbuf_fill.AddStorageReadonlyInput(common_buffers.instance_indices_res, Stg::VertexShader);

	const RpResRef shared_data_buf =
		gbuf_fill.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);

	const RpResRef cells_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.cells_res, Stg::FragmentShader);
	const RpResRef items_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.items_res, Stg::FragmentShader);
	const RpResRef decals_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.decals_res, Stg::FragmentShader);

	frame_textures.albedo = gbuf_fill.AddColorOutput(MAIN_ALBEDO_TEX, frame_textures.albedo_params);
	frame_textures.normal = gbuf_fill.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
	frame_textures.specular = gbuf_fill.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
	frame_textures.depth = gbuf_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

	rp_gbuffer_fill_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless,
		noise_tex, dummy_black, instances_buf, instances_indices_buf, shared_data_buf, cells_buf,
		items_buf, decals_buf, frame_textures.albedo, frame_textures.normal, frame_textures.specular,
		frame_textures.depth);
	gbuf_fill.set_executor(&rp_gbuffer_fill_);
}

void Eng::Renderer::AddForwardOpaquePass(const CommonBuffers& common_buffers, const PersistentGpuData& persistent_data,
	const BindlessTextureData& bindless, FrameTextures& frame_textures) {
	using Stg = Ren::eStageBits;

	auto& opaque = rp_builder_.AddPass("OPAQUE");
	const RpResRef vtx_buf1 = opaque.AddVertexBufferInput(ctx_.default_vertex_buf1());
	const RpResRef vtx_buf2 = opaque.AddVertexBufferInput(ctx_.default_vertex_buf2());
	const RpResRef ndx_buf = opaque.AddIndexBufferInput(ctx_.default_indices_buf());

	const RpResRef materials_buf = opaque.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(USE_GL_RENDER)
	const RpResRef textures_buf = opaque.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
	const RpResRef textures_buf = {};
#endif
	const RpResRef brdf_lut = opaque.AddTextureInput(brdf_lut_, Stg::FragmentShader);
	const RpResRef noise_tex = opaque.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
	const RpResRef cone_rt_lut = opaque.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

	const RpResRef dummy_black = opaque.AddTextureInput(dummy_black_, Stg::FragmentShader);

	const RpResRef instances_buf = opaque.AddStorageReadonlyInput(persistent_data.instance_buf,
		persistent_data.instance_buf_tbo, Stg::VertexShader);
	const RpResRef instances_indices_buf =
		opaque.AddStorageReadonlyInput(common_buffers.instance_indices_res, Stg::VertexShader);

	const RpResRef shader_data_buf =
		opaque.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);

	const RpResRef cells_buf = opaque.AddStorageReadonlyInput(common_buffers.cells_res, Stg::FragmentShader);
	const RpResRef items_buf = opaque.AddStorageReadonlyInput(common_buffers.items_res, Stg::FragmentShader);
	const RpResRef lights_buf = opaque.AddStorageReadonlyInput(common_buffers.lights_res, Stg::FragmentShader);
	const RpResRef decals_buf = opaque.AddStorageReadonlyInput(common_buffers.decals_res, Stg::FragmentShader);

	const RpResRef shadowmap_tex = opaque.AddTextureInput(frame_textures.shadowmap, Stg::FragmentShader);
	const RpResRef ssao_tex = opaque.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

	RpResRef lmap_tex[4];
	for (int i = 0; i < 4; ++i) {
		if (p_list_->env.lm_indir_sh[i]) {
			lmap_tex[i] = opaque.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
		}
	}

	frame_textures.color = opaque.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
	frame_textures.normal = opaque.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
	frame_textures.specular = opaque.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
	frame_textures.depth = opaque.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

	rp_opaque_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
		persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
		instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf,
		decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color, frame_textures.normal,
		frame_textures.specular, frame_textures.depth);
	opaque.set_executor(&rp_opaque_);
}

void Eng::Renderer::AddForwardTransparentPass(const CommonBuffers& common_buffers,
	const PersistentGpuData& persistent_data,
	const BindlessTextureData& bindless, FrameTextures& frame_textures) {
	using Stg = Ren::eStageBits;

	auto& transparent = rp_builder_.AddPass("TRANSPARENT");
	const RpResRef vtx_buf1 = transparent.AddVertexBufferInput(ctx_.default_vertex_buf1());
	const RpResRef vtx_buf2 = transparent.AddVertexBufferInput(ctx_.default_vertex_buf2());
	const RpResRef ndx_buf = transparent.AddIndexBufferInput(ctx_.default_indices_buf());

	const RpResRef materials_buf =
		transparent.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(USE_GL_RENDER)
	const RpResRef textures_buf = transparent.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
	const RpResRef textures_buf = {};
#endif
	const RpResRef brdf_lut = transparent.AddTextureInput(brdf_lut_, Stg::FragmentShader);
	const RpResRef noise_tex = transparent.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
	const RpResRef cone_rt_lut = transparent.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

	const RpResRef dummy_black = transparent.AddTextureInput(dummy_black_, Stg::FragmentShader);

	const RpResRef instances_buf = transparent.AddStorageReadonlyInput(
		persistent_data.instance_buf, persistent_data.instance_buf_tbo, Stg::VertexShader);
	const RpResRef instances_indices_buf =
		transparent.AddStorageReadonlyInput(common_buffers.instance_indices_res, Stg::VertexShader);

	const RpResRef shader_data_buf =
		transparent.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);

	const RpResRef cells_buf = transparent.AddStorageReadonlyInput(common_buffers.cells_res, Stg::FragmentShader);
	const RpResRef items_buf = transparent.AddStorageReadonlyInput(common_buffers.items_res, Stg::FragmentShader);
	const RpResRef lights_buf = transparent.AddStorageReadonlyInput(common_buffers.lights_res, Stg::FragmentShader);
	const RpResRef decals_buf = transparent.AddStorageReadonlyInput(common_buffers.decals_res, Stg::FragmentShader);

	const RpResRef shadowmap_tex = transparent.AddTextureInput(frame_textures.shadowmap, Stg::FragmentShader);
	const RpResRef ssao_tex = transparent.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

	RpResRef lmap_tex[4];
	for (int i = 0; i < 4; ++i) {
		if (p_list_->env.lm_indir_sh[i]) {
			lmap_tex[i] = transparent.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
		}
	}

	frame_textures.color = transparent.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
	frame_textures.normal = transparent.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
	frame_textures.specular = transparent.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
	frame_textures.depth = transparent.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

	rp_transparent_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
		persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
		instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf,
		decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color, frame_textures.normal,
		frame_textures.specular, frame_textures.depth);
	transparent.set_executor(&rp_transparent_);
}

void Eng::Renderer::AddDeferredShadingPass(const CommonBuffers& common_buffers, FrameTextures& frame_textures,
	bool enable_gi) {
	using Stg = Ren::eStageBits;

	auto& gbuf_shade = rp_builder_.AddPass("GBUFFER SHADE");

	struct PassData {
		RpResRef shared_data;
		RpResRef cells_buf, items_buf, lights_buf, decals_buf;
		RpResRef shadowmap_tex, ssao_tex, gi_tex, sun_shadow_tex;
		RpResRef depth_tex, albedo_tex, normal_tex, spec_tex;
		RpResRef ltc_diff_lut_tex[2], ltc_sheen_lut_tex[2], ltc_spec_lut_tex[2], ltc_coat_lut_tex[2];
		RpResRef output_tex;
	};

	auto* data = gbuf_shade.AllocPassData<PassData>();
	data->shared_data = gbuf_shade.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);

	data->cells_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.cells_res, Stg::ComputeShader);
	data->items_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.items_res, Stg::ComputeShader);
	data->lights_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.lights_res, Stg::ComputeShader);
	data->decals_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.decals_res, Stg::ComputeShader);

	data->shadowmap_tex = gbuf_shade.AddTextureInput(frame_textures.shadowmap, Stg::ComputeShader);
	data->ssao_tex = gbuf_shade.AddTextureInput(frame_textures.ssao, Stg::ComputeShader);
	if (enable_gi) {
		data->gi_tex = gbuf_shade.AddTextureInput(frame_textures.gi, Stg::ComputeShader);
	}
	else {
		data->gi_tex = gbuf_shade.AddTextureInput(dummy_black_, Stg::ComputeShader);
	}
	data->sun_shadow_tex = gbuf_shade.AddTextureInput(frame_textures.sun_shadow, Stg::ComputeShader);

	data->depth_tex = gbuf_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
	data->albedo_tex = gbuf_shade.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
	data->normal_tex = gbuf_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
	data->spec_tex = gbuf_shade.AddTextureInput(frame_textures.specular, Stg::ComputeShader);

	data->ltc_diff_lut_tex[0] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Diffuse][0], Stg::ComputeShader);
	data->ltc_diff_lut_tex[1] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Diffuse][1], Stg::ComputeShader);

	data->ltc_sheen_lut_tex[0] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Sheen][0], Stg::ComputeShader);
	data->ltc_sheen_lut_tex[1] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Sheen][1], Stg::ComputeShader);

	data->ltc_spec_lut_tex[0] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Specular][0], Stg::ComputeShader);
	data->ltc_spec_lut_tex[1] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Specular][1], Stg::ComputeShader);

	data->ltc_coat_lut_tex[0] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Clearcoat][0], Stg::ComputeShader);
	data->ltc_coat_lut_tex[1] = gbuf_shade.AddTextureInput(ltc_lut_[eLTCLut::Clearcoat][1], Stg::ComputeShader);

	frame_textures.color = data->output_tex =
		gbuf_shade.AddStorageImageOutput(MAIN_COLOR_TEX, frame_textures.color_params, Stg::ComputeShader);

	gbuf_shade.set_execute_cb([this, data](RpBuilder& builder) {
		RpAllocBuf& unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);
		RpAllocBuf& cells_buf = builder.GetReadBuffer(data->cells_buf);
		RpAllocBuf& items_buf = builder.GetReadBuffer(data->items_buf);
		RpAllocBuf& lights_buf = builder.GetReadBuffer(data->lights_buf);
		RpAllocBuf& decals_buf = builder.GetReadBuffer(data->decals_buf);

		RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
		RpAllocTex& albedo_tex = builder.GetReadTexture(data->albedo_tex);
		RpAllocTex& normal_tex = builder.GetReadTexture(data->normal_tex);
		RpAllocTex& spec_tex = builder.GetReadTexture(data->spec_tex);

		RpAllocTex& shad_tex = builder.GetReadTexture(data->shadowmap_tex);
		RpAllocTex& ssao_tex = builder.GetReadTexture(data->ssao_tex);
		RpAllocTex& gi_tex = builder.GetReadTexture(data->gi_tex);
		RpAllocTex& sun_shadow_tex = builder.GetReadTexture(data->sun_shadow_tex);

		RpAllocTex& ltc_diff_lut_tex0 = builder.GetReadTexture(data->ltc_diff_lut_tex[0]);
		RpAllocTex& ltc_diff_lut_tex1 = builder.GetReadTexture(data->ltc_diff_lut_tex[1]);

		RpAllocTex& ltc_sheen_lut_tex0 = builder.GetReadTexture(data->ltc_sheen_lut_tex[0]);
		RpAllocTex& ltc_sheen_lut_tex1 = builder.GetReadTexture(data->ltc_sheen_lut_tex[1]);

		RpAllocTex& ltc_spec_lut_tex0 = builder.GetReadTexture(data->ltc_spec_lut_tex[0]);
		RpAllocTex& ltc_spec_lut_tex1 = builder.GetReadTexture(data->ltc_spec_lut_tex[1]);

		RpAllocTex& ltc_coat_lut_tex0 = builder.GetReadTexture(data->ltc_coat_lut_tex[0]);
		RpAllocTex& ltc_coat_lut_tex1 = builder.GetReadTexture(data->ltc_coat_lut_tex[1]);

		RpAllocTex& out_color_tex = builder.GetWriteTexture(data->output_tex);

		const Ren::Binding bindings[] = {
			{Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_shared_data_buf.ref},
			{Ren::eBindTarget::TBuf, GBufferShade::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
			{Ren::eBindTarget::TBuf, GBufferShade::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
			{Ren::eBindTarget::TBuf, GBufferShade::LIGHT_BUF_SLOT, *lights_buf.tbos[0]},
			{Ren::eBindTarget::TBuf, GBufferShade::DECAL_BUF_SLOT, *decals_buf.tbos[0]},
			{Ren::eBindTarget::Tex2D, GBufferShade::DEPTH_TEX_SLOT, *depth_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::ALBEDO_TEX_SLOT, *albedo_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::NORMAL_TEX_SLOT, *normal_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::SPECULAR_TEX_SLOT, *spec_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::SHADOW_TEX_SLOT, *shad_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::SSAO_TEX_SLOT, *ssao_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::GI_TEX_SLOT, *gi_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::SUN_SHADOW_TEX_SLOT, *sun_shadow_tex.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_DIFF_LUT_TEX_SLOT, 0, *ltc_diff_lut_tex0.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_DIFF_LUT_TEX_SLOT, 1, *ltc_diff_lut_tex1.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_SHEEN_LUT_TEX_SLOT, 0, *ltc_sheen_lut_tex0.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_SHEEN_LUT_TEX_SLOT, 1, *ltc_sheen_lut_tex1.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_SPEC_LUT_TEX_SLOT, 0, *ltc_spec_lut_tex0.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_SPEC_LUT_TEX_SLOT, 1, *ltc_spec_lut_tex1.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_COAT_LUT_TEX_SLOT, 0, *ltc_coat_lut_tex0.ref},
			{Ren::eBindTarget::Tex2D, GBufferShade::LTC_COAT_LUT_TEX_SLOT, 1, *ltc_coat_lut_tex1.ref},
			{Ren::eBindTarget::Image, GBufferShade::OUT_COLOR_IMG_SLOT, *out_color_tex.ref} };

		const Ren::Vec3u grp_count = Ren::Vec3u{
			(view_state_.act_res[0] + GBufferShade::LOCAL_GROUP_SIZE_X - 1u) / GBufferShade::LOCAL_GROUP_SIZE_X,
			(view_state_.act_res[1] + GBufferShade::LOCAL_GROUP_SIZE_Y - 1u) / GBufferShade::LOCAL_GROUP_SIZE_Y, 1u };

		GBufferShade::Params uniform_params;
		uniform_params.img_size = Ren::Vec2u{ uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1]) };

		const Ren::Pipeline& pi = (render_flags_ & EnableHQ_HDR) ? pi_gbuf_shade_hq_ : pi_gbuf_shade_;
		Ren::DispatchCompute(pi, grp_count, bindings, &uniform_params, sizeof(uniform_params),
			builder.ctx().default_descr_alloc(), builder.ctx().log());
		});
}

void Eng::Renderer::AddSSAOPasses(const RpResRef depth_down_2x, const RpResRef _depth_tex, RpResRef& out_ssao) {
	const Ren::Vec4i cur_res =
		Ren::Vec4i{ view_state_.act_res[0], view_state_.act_res[1], view_state_.scr_res[0], view_state_.scr_res[1] };

	RpResRef ssao_raw;
	{ // Main SSAO pass
		auto& ssao = rp_builder_.AddPass("SSAO");

		struct PassData {
			RpResRef rand_tex;
			RpResRef depth_tex;

			RpResRef output_tex;
		};

		auto* data = ssao.AllocPassData<PassData>();
		data->rand_tex = ssao.AddTextureInput(rand2d_dirs_4x4_, Ren::eStageBits::FragmentShader);
		data->depth_tex = ssao.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);

		{ // Allocate output texture
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0] / 2;
			params.h = view_state_.scr_res[1] / 2;
			params.format = Ren::eTexFormat::RawR8;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			ssao_raw = data->output_tex = ssao.AddColorOutput(SSAO_RAW, params);
		}

		ssao.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& down_depth_2x_tex = builder.GetReadTexture(data->depth_tex);
			RpAllocTex& rand_tex = builder.GetReadTexture(data->rand_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0] / 2;
			rast_state.viewport[3] = view_state_.act_res[1] / 2;

			{ // prepare ao buffer
				const Ren::Binding bindings[] = {
					{Ren::eBindTarget::Tex2D, SSAO::DEPTH_TEX_SLOT, *down_depth_2x_tex.ref},
					{Ren::eBindTarget::Tex2D, SSAO::RAND_TEX_SLOT, *rand_tex.ref} };

				SSAO::Params uniform_params;
				uniform_params.transform =
					Ren::Vec4f{ 0.0f, 0.0f, view_state_.act_res[0] / 2, view_state_.act_res[1] / 2 };
				uniform_params.resolution = Ren::Vec2f{ float(view_state_.act_res[0]), float(view_state_.act_res[1]) };

				const Ren::RenderTarget render_targets[] = {
					{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

				prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ao_prog_, render_targets, {}, rast_state,
					builder.rast_state(), bindings, &uniform_params, sizeof(SSAO::Params), 0);
			}
			});
	}

	RpResRef ssao_blurred1;
	{ // Horizontal SSAO blur
		auto& ssao_blur_h = rp_builder_.AddPass("SSAO BLUR H");

		struct PassData {
			RpResRef depth_tex;
			RpResRef input_tex;

			RpResRef output_tex;
		};

		auto* data = ssao_blur_h.AllocPassData<PassData>();
		data->depth_tex = ssao_blur_h.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
		data->input_tex = ssao_blur_h.AddTextureInput(ssao_raw, Ren::eStageBits::FragmentShader);

		{ // Allocate output texture
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0] / 2;
			params.h = view_state_.scr_res[1] / 2;
			params.format = Ren::eTexFormat::RawR8;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			ssao_blurred1 = data->output_tex = ssao_blur_h.AddColorOutput("SSAO BLUR TEMP1", params);
		}

		ssao_blur_h.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
			RpAllocTex& input_tex = builder.GetReadTexture(data->input_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0] / 2;
			rast_state.viewport[3] = view_state_.act_res[1] / 2;

			{ // blur ao buffer
				const Ren::RenderTarget render_targets[] = {
					{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

				const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, Bilateral::DEPTH_TEX_SLOT, *depth_tex.ref},
												 {Ren::eBindTarget::Tex2D, Bilateral::INPUT_TEX_SLOT, *input_tex.ref} };

				Bilateral::Params uniform_params;
				uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, 1.0f, 1.0f };
				uniform_params.resolution = Ren::Vec2f{ float(rast_state.viewport[2]), float(rast_state.viewport[3]) };
				uniform_params.vertical = 0.0f;

				prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, render_targets, {}, rast_state,
					builder.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params), 0);
			}
			});
	}

	RpResRef ssao_blurred2;
	{ // Vertical SSAO blur
		auto& ssao_blur_v = rp_builder_.AddPass("SSAO BLUR V");

		struct PassData {
			RpResRef depth_tex;
			RpResRef input_tex;

			RpResRef output_tex;
		};

		auto* data = ssao_blur_v.AllocPassData<PassData>();
		data->depth_tex = ssao_blur_v.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
		data->input_tex = ssao_blur_v.AddTextureInput(ssao_blurred1, Ren::eStageBits::FragmentShader);

		{ // Allocate output texture
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0] / 2;
			params.h = view_state_.scr_res[1] / 2;
			params.format = Ren::eTexFormat::RawR8;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			ssao_blurred2 = data->output_tex = ssao_blur_v.AddColorOutput("SSAO BLUR TEMP2", params);
		}

		ssao_blur_v.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
			RpAllocTex& input_tex = builder.GetReadTexture(data->input_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0] / 2;
			rast_state.viewport[3] = view_state_.act_res[1] / 2;

			{ // blur ao buffer
				const Ren::RenderTarget render_targets[] = {
					{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

				const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, Bilateral::DEPTH_TEX_SLOT, *depth_tex.ref},
												 {Ren::eBindTarget::Tex2D, Bilateral::INPUT_TEX_SLOT, *input_tex.ref} };

				Bilateral::Params uniform_params;
				uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, 1.0f, 1.0f };
				uniform_params.resolution = Ren::Vec2f{ float(rast_state.viewport[2]), float(rast_state.viewport[3]) };
				uniform_params.vertical = 1.0f;

				prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, render_targets, {}, rast_state,
					builder.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params), 0);
			}
			});
	}

	{ // Upscale SSAO pass
		auto& ssao_upscale = rp_builder_.AddPass("UPSCALE");

		struct PassData {
			RpResRef depth_down_2x_tex;
			RpResRef depth_tex;
			RpResRef input_tex;

			RpResRef output_tex;
		};

		auto* data = ssao_upscale.AllocPassData<PassData>();
		data->depth_down_2x_tex = ssao_upscale.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
		data->depth_tex = ssao_upscale.AddTextureInput(_depth_tex, Ren::eStageBits::FragmentShader);
		data->input_tex = ssao_upscale.AddTextureInput(ssao_blurred2, Ren::eStageBits::FragmentShader);

		{ // Allocate output texture
			Ren::Tex2DParams params;
			params.w = view_state_.act_res[0];
			params.h = view_state_.act_res[1];
			params.format = Ren::eTexFormat::RawR8;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			out_ssao = data->output_tex = ssao_upscale.AddColorOutput(SSAO_RES, params);
		}

		ssao_upscale.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& down_depth_2x_tex = builder.GetReadTexture(data->depth_down_2x_tex);
			RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
			RpAllocTex& input_tex = builder.GetReadTexture(data->input_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
			rast_state.viewport[2] = view_state_.act_res[0];
			rast_state.viewport[3] = view_state_.act_res[1];
			{ // upsample ao
				const Ren::RenderTarget render_targets[] = {
					{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };
				const Ren::Binding bindings[] = {
					{Ren::eBindTarget::Tex2D, Upscale::DEPTH_TEX_SLOT, *depth_tex.ref},
					{Ren::eBindTarget::Tex2D, Upscale::DEPTH_LOW_TEX_SLOT, *down_depth_2x_tex.ref},
					{Ren::eBindTarget::Tex2D, Upscale::INPUT_TEX_SLOT, *input_tex.ref} };
				Upscale::Params uniform_params;
				uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, 1.0f, 1.0f };
				uniform_params.resolution = Ren::Vec4f{ float(view_state_.act_res[0]), float(view_state_.act_res[1]),
													   float(view_state_.scr_res[0]), float(view_state_.scr_res[1]) };
				uniform_params.clip_info = view_state_.clip_info;
				prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_upscale_prog_, render_targets, {}, rast_state,
					builder.rast_state(), bindings, &uniform_params, sizeof(Upscale::Params), 0);
			}
			});
	}
}

void Eng::Renderer::AddFillStaticVelocityPass(const CommonBuffers& common_buffers, RpResRef depth_tex,
	RpResRef& inout_velocity_tex) {
	assert(!view_state_.is_multisampled);
	auto& static_vel = rp_builder_.AddPass("FILL STATIC VEL");

	struct PassData {
		RpResRef shared_data;
		RpResRef depth_tex;
		RpResRef velocity_tex;
	};

	auto* data = static_vel.AllocPassData<PassData>();

	data->shared_data = static_vel.AddUniformBufferInput(
		common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
	data->depth_tex =
		static_vel.AddCustomTextureInput(depth_tex, Ren::eResState::StencilTestDepthFetch,
			Ren::eStageBits::DepthAttachment | Ren::eStageBits::FragmentShader);
	inout_velocity_tex = data->velocity_tex = static_vel.AddColorOutput(inout_velocity_tex);

	static_vel.set_execute_cb([this, data](RpBuilder& builder) {
		RpAllocBuf& unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);

		RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
		RpAllocTex& velocity_tex = builder.GetWriteTexture(data->velocity_tex);

		Ren::RastState rast_state;
		rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
		rast_state.stencil.enabled = true;
		rast_state.stencil.write_mask = 0x00;
		rast_state.stencil.compare_op = unsigned(Ren::eCompareOp::Equal);

		rast_state.viewport[2] = view_state_.act_res[0];
		rast_state.viewport[3] = view_state_.act_res[1];

		const Ren::Binding bindings[] = {
			{Ren::eBindTarget::Tex2D, BlitStaticVel::DEPTH_TEX_SLOT, *depth_tex.ref},
			{Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref} };

		const Ren::RenderTarget render_targets[] = { {velocity_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store} };
		const Ren::RenderTarget depth_target = { depth_tex.ref, Ren::eLoadOp::None, Ren::eStoreOp::None,
												Ren::eLoadOp::Load, Ren::eStoreOp::None };

		BlitStaticVel::Params uniform_params;
		uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, float(view_state_.act_res[0]), float(view_state_.act_res[1]) };

		prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_static_vel_prog_, render_targets, depth_target, rast_state,
			builder.rast_state(), bindings, &uniform_params, sizeof(BlitStaticVel::Params), 0);
		});
}

void Eng::Renderer::AddFrameBlurPasses(const Ren::WeakTex2DRef& input_tex, RpResRef& output_tex) {
	RpResRef blur_temp;
	{ // Blur frame horizontally
		auto& blur_h = rp_builder_.AddPass("BLUR H");

		struct PassData {
			RpResRef input_tex;
			RpResRef output_tex;
		};

		auto* data = blur_h.AllocPassData<PassData>();
		data->input_tex = blur_h.AddTextureInput(input_tex, Ren::eStageBits::FragmentShader);

		{ //
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0] / 4;
			params.h = view_state_.scr_res[1] / 4;
			params.format = Ren::eTexFormat::RawRG11F_B10F;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			blur_temp = data->output_tex = blur_h.AddColorOutput("Blur temp", params);
		}

		blur_h.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& intput_tex = builder.GetReadTexture(data->input_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0] / 4;
			rast_state.viewport[3] = view_state_.act_res[1] / 4;

			const Ren::RenderTarget render_targets[] = { {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

			const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, Gauss::SRC_TEX_SLOT, *intput_tex.ref} };

			Gauss::Params uniform_params;
			uniform_params.transform =
				Ren::Vec4f{ 0.0f, 0.0f, float(rast_state.viewport[2]), float(rast_state.viewport[3]) };
			uniform_params.vertical[0] = 0.0f;

			prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_gauss2_prog_, render_targets, {}, rast_state,
				builder.rast_state(), bindings, &uniform_params, sizeof(Gauss::Params), 0);
			});
	}
	{ // Blur frame vertically
		auto& blur_v = rp_builder_.AddPass("BLUR V");

		struct PassData {
			RpResRef input_tex;
			RpResRef output_tex;
		};

		auto* data = blur_v.AllocPassData<PassData>();
		data->input_tex = blur_v.AddTextureInput(blur_temp, Ren::eStageBits::FragmentShader);

		{ //
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0] / 4;
			params.h = view_state_.scr_res[1] / 4;
			params.format = Ren::eTexFormat::RawRG11F_B10F;
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			output_tex = data->output_tex = blur_v.AddColorOutput(BLUR_RES_TEX, params);
		}

		blur_v.set_execute_cb([this, data](RpBuilder& builder) {
			RpAllocTex& intput_tex = builder.GetReadTexture(data->input_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0] / 4;
			rast_state.viewport[3] = view_state_.act_res[1] / 4;

			const Ren::RenderTarget render_targets[] = { {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

			const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, Gauss::SRC_TEX_SLOT, *intput_tex.ref} };

			Gauss::Params uniform_params;
			uniform_params.transform =
				Ren::Vec4f{ 0.0f, 0.0f, float(rast_state.viewport[2]), float(rast_state.viewport[3]) };
			uniform_params.vertical[0] = 1.0f;

			prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_gauss2_prog_, render_targets, {}, rast_state,
				builder.rast_state(), bindings, &uniform_params, sizeof(Gauss::Params), 0);
			});
	}
}

void Eng::Renderer::AddTaaPass(const CommonBuffers& common_buffers, FrameTextures& frame_textures,
	const float max_exposure, const bool static_accumulation, RpResRef& resolved_color) {
	assert(!view_state_.is_multisampled);
	{ // TAA
		auto& taa = rp_builder_.AddPass("TAA");

		struct PassData {
			RpResRef shared_data;

			RpResRef clean_tex;
			RpResRef depth_tex;
			RpResRef velocity_tex;
			RpResRef history_tex;

			RpResRef output_tex;
			RpResRef output_history_tex;
		};

		auto* data = taa.AllocPassData<PassData>();
		data->shared_data = taa.AddUniformBufferInput(common_buffers.shared_data_res,
			Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
		data->clean_tex = taa.AddTextureInput(frame_textures.color, Ren::eStageBits::FragmentShader);
		data->depth_tex = taa.AddTextureInput(frame_textures.depth, Ren::eStageBits::FragmentShader);
		data->velocity_tex = taa.AddTextureInput(frame_textures.velocity, Ren::eStageBits::FragmentShader);

		{ // Texture that holds resolved color
			Ren::Tex2DParams params;
			params.w = view_state_.scr_res[0];
			params.h = view_state_.scr_res[1];
			if (render_flags_ & EnableHQ_HDR) {
				params.format = Ren::eTexFormat::RawRGBA16F;
			}
			else {
				params.format = Ren::eTexFormat::RawRG11F_B10F;
			}
			params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
			params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

			resolved_color = data->output_tex = taa.AddColorOutput(RESOLVED_COLOR_TEX, params);
			data->output_history_tex = taa.AddColorOutput("Color History", params);
		}
		data->history_tex = taa.AddHistoryTextureInput(data->output_history_tex, Ren::eStageBits::FragmentShader);

		taa.set_execute_cb([this, data, max_exposure, static_accumulation](RpBuilder& builder) {
			RpAllocBuf& unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);

			RpAllocTex& clean_tex = builder.GetReadTexture(data->clean_tex);
			RpAllocTex& depth_tex = builder.GetReadTexture(data->depth_tex);
			RpAllocTex& velocity_tex = builder.GetReadTexture(data->velocity_tex);
			RpAllocTex& history_tex = builder.GetReadTexture(data->history_tex);
			RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);
			RpAllocTex& output_history_tex = builder.GetWriteTexture(data->output_history_tex);

			Ren::RastState rast_state;
			rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

			rast_state.viewport[2] = view_state_.act_res[0];
			rast_state.viewport[3] = view_state_.act_res[1];

			{ // Blit taa
				const Ren::RenderTarget render_targets[] = {
					{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store},
					{output_history_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

				// exposure from previous frame
				float exposure =
					reduced_average_ > std::numeric_limits<float>::epsilon() ? (1.0f / reduced_average_) : 1.0f;
				exposure = std::min(exposure, max_exposure);

				const Ren::Binding bindings[] = {
					{Ren::eBindTarget::Tex2D, TempAA::CURR_TEX_SLOT, *clean_tex.ref},
					{Ren::eBindTarget::Tex2D, TempAA::HIST_TEX_SLOT, *history_tex.ref},
					{Ren::eBindTarget::Tex2D, TempAA::DEPTH_TEX_SLOT, *depth_tex.ref},
					{Ren::eBindTarget::Tex2D, TempAA::VELOCITY_TEX_SLOT, *velocity_tex.ref} };

				TempAA::Params uniform_params;
				uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, view_state_.act_res[0], view_state_.act_res[1] };
				uniform_params.tex_size = Ren::Vec2f{ float(view_state_.act_res[0]), float(view_state_.act_res[1]) };
				uniform_params.exposure = exposure;
				if (static_accumulation && int(accumulated_frames_) < RendererInternal::TaaSampleCountStatic) {
					uniform_params.mix_factor = 1.0f / (1.0f + accumulated_frames_);
				}
				else {
					uniform_params.mix_factor = 0.0f;
				}
				++accumulated_frames_;

				prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, static_accumulation ? blit_taa_static_prog_ : blit_taa_prog_,
					render_targets, {}, rast_state, builder.rast_state(), bindings, &uniform_params,
					sizeof(TempAA::Params), 0);
			}
			});
	}
}

void Eng::Renderer::AddDownsampleColorPass(RpResRef input_tex, RpResRef& output_tex) {
	auto& down_color = rp_builder_.AddPass("DOWNSAMPLE COLOR");

	struct PassData {
		RpResRef input_tex;
		RpResRef output_tex;
	};

	auto* data = down_color.AllocPassData<PassData>();
	data->input_tex = down_color.AddTextureInput(input_tex, Ren::eStageBits::FragmentShader);
	output_tex = data->output_tex = down_color.AddColorOutput(output_tex);

	down_color.set_execute_cb([this, data](RpBuilder& builder) {
		RpAllocTex& input_tex = builder.GetReadTexture(data->input_tex);
		RpAllocTex& output_tex = builder.GetWriteTexture(data->output_tex);

		Ren::RastState rast_state;

		rast_state.viewport[2] = view_state_.act_res[0] / 4;
		rast_state.viewport[3] = view_state_.act_res[1] / 4;

		const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, DownColor::SRC_TEX_SLOT, *input_tex.ref} };

		DownColor::Params uniform_params;
		uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, float(view_state_.act_res[0]) / float(view_state_.scr_res[0]),
											  float(view_state_.act_res[1]) / float(view_state_.scr_res[1]) };
		uniform_params.resolution =
			Ren::Vec4f{ float(view_state_.act_res[0]), float(view_state_.act_res[1]), 0.0f, 0.0f };

		const Ren::RenderTarget render_targets[] = { {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

		prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down2_prog_, render_targets, {}, rast_state,
			builder.rast_state(), bindings, &uniform_params, sizeof(DownColor::Params), 0);
		});
}

void Eng::Renderer::AddDownsampleDepthPass(const CommonBuffers& common_buffers, RpResRef depth_tex,
	RpResRef& out_depth_down_2x) {
	auto& downsample_depth = rp_builder_.AddPass("DOWN DEPTH");

	struct PassData {
		RpResRef shared_data;
		RpResRef in_depth_tex;

		RpResRef out_depth_tex;
	};

	auto* data = downsample_depth.AllocPassData<PassData>();
	data->shared_data = downsample_depth.AddUniformBufferInput(
		common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
	data->in_depth_tex = downsample_depth.AddTextureInput(depth_tex, Ren::eStageBits::FragmentShader);

	{ // Texture that holds 2x downsampled linear depth
		Ren::Tex2DParams params;
		params.w = view_state_.scr_res[0] / 2;
		params.h = view_state_.scr_res[1] / 2;
		params.format = Ren::eTexFormat::RawR32F;
		params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

		out_depth_down_2x = data->out_depth_tex = downsample_depth.AddColorOutput(DEPTH_DOWN_2X_TEX, params);
	}

	downsample_depth.set_execute_cb([this, data](RpBuilder& builder) {
		RpAllocBuf& unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);
		RpAllocTex& depth_tex = builder.GetReadTexture(data->in_depth_tex);
		RpAllocTex& output_tex = builder.GetWriteTexture(data->out_depth_tex);

		Ren::RastState rast_state;

		rast_state.viewport[2] = view_state_.act_res[0] / 2;
		rast_state.viewport[3] = view_state_.act_res[1] / 2;

		const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, DownDepth::DEPTH_TEX_SLOT, *depth_tex.ref} };

		DownDepth::Params uniform_params;
		uniform_params.transform = Ren::Vec4f{ 0.0f, 0.0f, float(view_state_.act_res[0]), float(view_state_.act_res[1]) };
		uniform_params.clip_info = view_state_.clip_info;
		uniform_params.linearize = 1.0f;

		const Ren::RenderTarget render_targets[] = { {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store} };

		prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down_depth_prog_, render_targets, {}, rast_state,
			builder.rast_state(), bindings, &uniform_params, sizeof(DownDepth::Params), 0);
		});
}

void Eng::Renderer::AddDebugVelocityPass(const RpResRef velocity, RpResRef& output_tex) {
	auto& debug_motion = rp_builder_.AddPass("DEBUG MOTION");

	struct PassData {
		RpResRef in_velocity_tex;
		RpResRef out_color_tex;
	};

	auto* data = debug_motion.AllocPassData<PassData>();
	data->in_velocity_tex = debug_motion.AddTextureInput(velocity, Ren::eStageBits::ComputeShader);

	{ // Output texture
		Ren::Tex2DParams params;
		params.w = view_state_.scr_res[0];
		params.h = view_state_.scr_res[1];
		params.format = Ren::eTexFormat::RawRGBA8888;
		params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

		output_tex = data->out_color_tex =
			debug_motion.AddStorageImageOutput("Velocity Debug", params, Ren::eStageBits::ComputeShader);
	}

	debug_motion.set_execute_cb([this, data](RpBuilder& builder) {
		RpAllocTex& velocity_tex = builder.GetReadTexture(data->in_velocity_tex);
		RpAllocTex& output_tex = builder.GetWriteTexture(data->out_color_tex);

		const Ren::Binding bindings[] = { {Ren::eBindTarget::Tex2D, DebugVelocity::VELOCITY_TEX_SLOT, *velocity_tex.ref},
										 {Ren::eBindTarget::Image, DebugVelocity::OUT_IMG_SLOT, *output_tex.ref} };

		const Ren::Vec3u grp_count = Ren::Vec3u{
			(view_state_.act_res[0] + DebugVelocity::LOCAL_GROUP_SIZE_X - 1u) / DebugVelocity::LOCAL_GROUP_SIZE_X,
			(view_state_.act_res[1] + DebugVelocity::LOCAL_GROUP_SIZE_Y - 1u) / DebugVelocity::LOCAL_GROUP_SIZE_Y, 1u };

		DebugVelocity::Params uniform_params;
		uniform_params.img_size[0] = view_state_.act_res[0];
		uniform_params.img_size[1] = view_state_.act_res[1];

		Ren::DispatchCompute(pi_debug_velocity_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
			ctx_.default_descr_alloc(), ctx_.log());
		});
}