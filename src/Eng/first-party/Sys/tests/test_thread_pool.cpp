#include "test_common.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../ThreadPool.h"

void test_thread_pool() {
    using namespace Sys;

    printf("Test thread_pool        | ");

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
        TaskList task_list;

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

        ThreadPool threads(16);
        threads.Enqueue(std::move(task_list)).wait();

        require(A_finished && B_finished && C_finished && D_finished && E_finished && F_finished && G_finished &&
                H_finished && I_finished && J_finished);
    }

    { // test 'close' sorting
        TaskList task_list;

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

        ThreadPool threads(16);
        threads.Enqueue(std::move(task_list)).wait();

        require(A_finished && B_finished && C_finished && D_finished && E_finished && F_finished && G_finished &&
                H_finished && I_finished && J_finished);
    }

    { // parallel for wrapper
        int data[128] = {};

        ThreadPool threads(16);
        threads.ParallelFor(0, 64, [&](const int i) { ++data[i]; });
        threads.ParallelFor(64, 128, [&](const int i) { ++data[i]; });

        for (int i = 0; i < 128; ++i) {
            require(data[i] == 1);
        }
    }

    { // AddDependency edge cases: self and duplicate are silently ignored
        TaskList task_list;
        const short A = task_list.AddTask([]() {});
        const short B = task_list.AddTask([]() {});

        require(!task_list.AddDependency(A, A));   // self-dependency rejected
        require(task_list.AddDependency(B, A));    // first time: accepted
        require(!task_list.AddDependency(B, A));   // duplicate: rejected
        require(task_list.tasks[B].dependencies == 1);
    }

    { // HasCycles detects a directed cycle
        TaskList task_list;
        const short A = task_list.AddTask([]() {});
        const short B = task_list.AddTask([]() {});
        const short C = task_list.AddTask([]() {});
        task_list.AddDependency(B, A);
        task_list.AddDependency(C, B);
        task_list.AddDependency(A, C); // cycle: A -> B -> C -> A
        task_list.Sort();
        require(task_list.HasCycles());
    }

    { // linear chain A->B->C->D executes in strict order
        TaskList task_list;
        int seq = 0;
        int A_seq = -1, B_seq = -1, C_seq = -1, D_seq = -1;

        const short A = task_list.AddTask([&]() { A_seq = seq++; });
        const short B = task_list.AddTask([&]() { B_seq = seq++; });
        const short C = task_list.AddTask([&]() { C_seq = seq++; });
        const short D = task_list.AddTask([&]() { D_seq = seq++; });
        task_list.AddDependency(B, A);
        task_list.AddDependency(C, B);
        task_list.AddDependency(D, C);
        task_list.Sort();
        require(!task_list.HasCycles());
        require(task_list.tasks_order[0] == A);
        require(task_list.tasks_order[1] == B);
        require(task_list.tasks_order[2] == C);
        require(task_list.tasks_order[3] == D);

        ThreadPool threads(4);
        threads.Enqueue(std::move(task_list)).wait();
        require(A_seq == 0 && B_seq == 1 && C_seq == 2 && D_seq == 3);
    }

    { // workers_count matches constructor argument
        require(ThreadPool(1).workers_count() == 1);
        require(ThreadPool(4).workers_count() == 4);
    }

    { // Enqueue with return value yields correct typed future
        ThreadPool threads(2);
        auto f = threads.Enqueue([]() { return 42; });
        require(f.get() == 42);
    }

    { // copy Enqueue: same TaskList can be submitted multiple times
        TaskList task_list;
        std::atomic_int counter{0};
        for (int i = 0; i < 4; ++i) {
            task_list.AddTask([&]() { ++counter; });
            task_list.tasks_order.push_back(short(i));
        }

        ThreadPool threads(4);
        threads.Enqueue(task_list).wait();    // copy
        require(counter == 4);
        require(task_list.tasks.size() == 4); // original unmodified

        counter = 0;
        threads.Enqueue(task_list).wait();    // copy again
        require(counter == 4);
    }

    { // multiple independent TaskLists all complete
        const int LISTS = 4, TASKS_EACH = 8;
        std::atomic_int total{0};

        ThreadPool threads(4);
        std::vector<std::future<void>> futures;
        for (int l = 0; l < LISTS; ++l) {
            TaskList list;
            for (int t = 0; t < TASKS_EACH; ++t) {
                list.AddTask([&]() { ++total; });
                list.tasks_order.push_back(short(t));
            }
            futures.push_back(threads.Enqueue(std::move(list)));
        }
        for (auto &f : futures) {
            f.wait();
        }
        require(total == LISTS * TASKS_EACH);
    }

    { // ParallelFor: empty and reverse ranges are no-ops
        ThreadPool threads(4);
        int x = 0;
        require_nothrow(threads.ParallelFor(5, 5, [&](int) { ++x; }));
        require(x == 0);
        require_nothrow(threads.ParallelFor(10, 5, [&](int) { ++x; }));
        require(x == 0);
    }

    { // tasks without dependencies actually run concurrently
        const int N = 4;
        std::atomic_int concurrent{0};
        std::atomic_int peak{0};

        TaskList task_list;
        for (int i = 0; i < N; ++i) {
            task_list.AddTask([&]() {
                const int c = ++concurrent;
                int prev = peak.load(std::memory_order_relaxed);
                while (c > prev && !peak.compare_exchange_weak(prev, c, std::memory_order_relaxed)) {}
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                --concurrent;
            });
            task_list.tasks_order.push_back(short(i));
        }

        ThreadPool threads(N);
        threads.Enqueue(std::move(task_list)).wait();
        require(peak > 1);
    }

    printf("OK\n");
}
