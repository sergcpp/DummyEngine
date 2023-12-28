#include "test_common.h"

#include <iostream>
#include <string>

#include "../ThreadPool.h"

void test_thread_pool() {
    //
    // [A]
    //    \
    //     [E]
    //    /   \
    // [B]     [H]
    //    \   /   \
    //     [F]     [J]
    //    /   \   /
    // [C]     [I]
    //    \   /
    //     [G]
    //    /
    // [D]
    //
    // Normal sorting:       [A][B][C][D][E][F][G][H][I][J]
    // 'Keep close' sorting: [A][B][E][C][F][H][D][G][I][J]

    { // test normal sorting
        Sys::TaskList task_list;

        bool A_finished = false, B_finished = false, C_finished = false, D_finished = false, E_finished = false,
             F_finished = false, G_finished = false, H_finished = false, I_finished = false, J_finished = false;

        const short J_id = task_list.AddTask([&]() {
            require(H_finished && I_finished);
            J_finished = true;
        });

        const short H_id = task_list.AddTask([&]() {
            require(E_finished && F_finished);
            H_finished = true;
        });
        const short I_id = task_list.AddTask([&]() {
            require(F_finished && G_finished);
            I_finished = true;
        });

        const short E_id = task_list.AddTask([&]() {
            require(A_finished && B_finished);
            E_finished = true;
        });
        const short F_id = task_list.AddTask([&]() {
            require(B_finished && C_finished);
            F_finished = true;
        });
        const short G_id = task_list.AddTask([&]() {
            require(C_finished && D_finished);
            G_finished = true;
        });

        const short A_id = task_list.AddTask([&]() { A_finished = true; });
        const short B_id = task_list.AddTask([&]() { B_finished = true; });
        const short C_id = task_list.AddTask([&]() { C_finished = true; });
        const short D_id = task_list.AddTask([&]() { D_finished = true; });

        task_list.AddDependency(E_id, A_id);
        task_list.AddDependency(E_id, B_id);
        task_list.AddDependency(F_id, B_id);
        task_list.AddDependency(F_id, C_id);
        task_list.AddDependency(G_id, C_id);
        task_list.AddDependency(G_id, D_id);

        task_list.AddDependency(H_id, E_id);
        task_list.AddDependency(H_id, F_id);
        task_list.AddDependency(I_id, F_id);
        task_list.AddDependency(I_id, G_id);

        task_list.AddDependency(J_id, H_id);
        task_list.AddDependency(J_id, I_id);

        task_list.Sort();
        require(!task_list.HasCycles());
        require(task_list.tasks_order[0] == A_id);
        require(task_list.tasks_order[1] == B_id);
        require(task_list.tasks_order[2] == C_id);
        require(task_list.tasks_order[3] == D_id);

        require(task_list.tasks_order[4] == E_id);
        require(task_list.tasks_order[5] == F_id);
        require(task_list.tasks_order[6] == G_id);

        require(task_list.tasks_order[7] == H_id);
        require(task_list.tasks_order[8] == I_id);

        require(task_list.tasks_order[9] == J_id);

        Sys::ThreadPool threads(16);
        threads.Enqueue(std::move(task_list)).wait();

        require(A_finished && B_finished && C_finished && D_finished && E_finished && F_finished && G_finished &&
                H_finished && I_finished && J_finished);
    }

    { // test 'close' sorting
        Sys::TaskList task_list;

        bool A_finished = false, B_finished = false, C_finished = false, D_finished = false, E_finished = false,
             F_finished = false, G_finished = false, H_finished = false, I_finished = false, J_finished = false;

        const short J_id = task_list.AddTask([&]() {
            require(H_finished && I_finished);
            J_finished = true;
        });

        const short H_id = task_list.AddTask([&]() {
            require(E_finished && F_finished);
            H_finished = true;
        });
        const short I_id = task_list.AddTask([&]() {
            require(F_finished && G_finished);
            I_finished = true;
        });

        const short E_id = task_list.AddTask([&]() {
            require(A_finished && B_finished);
            E_finished = true;
        });
        const short F_id = task_list.AddTask([&]() {
            require(B_finished && C_finished);
            F_finished = true;
        });
        const short G_id = task_list.AddTask([&]() {
            require(C_finished && D_finished);
            G_finished = true;
        });

        const short A_id = task_list.AddTask([&]() { A_finished = true; });
        const short B_id = task_list.AddTask([&]() { B_finished = true; });
        const short C_id = task_list.AddTask([&]() { C_finished = true; });
        const short D_id = task_list.AddTask([&]() { D_finished = true; });

        task_list.AddDependency(E_id, A_id);
        task_list.AddDependency(E_id, B_id);
        task_list.AddDependency(F_id, B_id);
        task_list.AddDependency(F_id, C_id);
        task_list.AddDependency(G_id, C_id);
        task_list.AddDependency(G_id, D_id);

        task_list.AddDependency(H_id, E_id);
        task_list.AddDependency(H_id, F_id);
        task_list.AddDependency(I_id, F_id);
        task_list.AddDependency(I_id, G_id);

        task_list.AddDependency(J_id, H_id);
        task_list.AddDependency(J_id, I_id);
        task_list.Sort(true /* keep_close */);
        require(!task_list.HasCycles());

        require(task_list.tasks_order[0] == A_id);
        require(task_list.tasks_order[1] == B_id);
        require(task_list.tasks_order[2] == E_id);
        require(task_list.tasks_order[3] == C_id);
        require(task_list.tasks_order[4] == F_id);
        require(task_list.tasks_order[5] == H_id);
        require(task_list.tasks_order[6] == D_id);
        require(task_list.tasks_order[7] == G_id);
        require(task_list.tasks_order[8] == I_id);
        require(task_list.tasks_order[9] == J_id);

        Sys::ThreadPool threads(16);
        threads.Enqueue(std::move(task_list)).wait();

        require(A_finished && B_finished && C_finished && D_finished && E_finished && F_finished && G_finished &&
                H_finished && I_finished && J_finished);
    }

    { // parallel for wrapper
        int data[128] = {};

        Sys::ThreadPool threads(16);
        threads.ParallelFor(0, 64, [&](const int i) { ++data[i]; });
        threads.ParallelFor(64, 128, [&](const int i) { ++data[i]; });

        for (int i = 0; i < 128; ++i) {
            require(data[i] == 1);
        }
    }
}
