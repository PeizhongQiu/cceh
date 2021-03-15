使用可拓展的哈希结构，只有一层，且现在为单核，即无并发的情况

insert：通过segment中的位图直接判断哪个位置有空缺，最简单的办法求解位图中最高位为1的位置。
        找到后，在对应位置插入，并修改位图。

resize：位图全为0即为满，判断local_depth和global_depth大小。
        local < global 创建new_seg，并将要old_seg中的元素符合条件的插入到new_seg，更改dir的指针，更新old_seg中的位图，最后更新old_seg的local_depth
        local = global 