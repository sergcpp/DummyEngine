#include "RpBuildAccStructures.h"

void RpBuildAccStructures::Setup(RpBuilder &builder, const char rt_obj_instances_buf[], const uint32_t instance_count,
                                 const char rt_tlas_scratch_buf_name[],
                                 const AccelerationStructureData *acc_struct_data) {
    instance_count_ = instance_count;
    acc_struct_data_ = acc_struct_data;

    rt_obj_instances_buf_ = builder.ReadBuffer(rt_obj_instances_buf, Ren::eResState::BuildASRead,
                                                Ren::eStageBits::AccStructureBuild, *this);
    rt_tlas_buf_ = builder.WriteBuffer(acc_struct_data->rt_tlas_buf, Ren::eResState::BuildASWrite,
                                       Ren::eStageBits::AccStructureBuild, *this);

    { // create scratch buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = acc_struct_data->rt_tlas_build_scratch_size;
        rt_tlas_build_scratch_buf_ = builder.WriteBuffer(rt_tlas_scratch_buf_name, desc, Ren::eResState::BuildASWrite,
                                                         Ren::eStageBits::AccStructureBuild, *this);
    }
}