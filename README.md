# cgv2-stat-percpu_test

cgroup v2 per-cpu test helper

* Environment preparation:

  1、Recompile and install the [patched](https://lore.kernel.org/all/20230717093612.40846-1-jiahao.os@bytedance.com/) kernel

  2、Compile and install the kernel module:
```
  make ; insmod cgv2_stat_percpu.ko
```

* Testing process:

1. Create a test cgroup

```
mkdir /sys/fs/cgroup/test
echo "+cpu" > /sys/fs/cgroup/test/cgroup.subtree_control
mkdir /sys/fs/cgroup/test/t1 ; mkdir /sys/fs/cgroup/test/t2
```

2. Add some test threads to the cgroup

```
echo $$ > /sys/fs/cgroup/test/t1/cgroup.procs ; stress -c 5&
echo $$ > /sys/fs/cgroup/test/t2/cgroup.procs ; stress -c 10&
```
3. Remove all threads to **keep the data stable**

```
pkill stress ; echo $$ > /sys/fs/cgroup/init.scope/cgroup.procs
```

4. View and compare test results

**NOTE:** We need to flush the data before "cat cpu.stat_percpu_all", such as "cat cpu.stat".

```
cat /sys/fs/cgroup/test/cpu.stat ; echo "\n" ; cat /sys/fs/cgroup/test/cpu.stat_percpu_all
cat /sys/fs/cgroup/test/t1/cpu.stat ; echo "\n" ;cat /sys/fs/cgroup/test/t1/cpu.stat_percpu_all
cat /sys/fs/cgroup/test/t2/cpu.stat ; echo "\n" ;cat /sys/fs/cgroup/test/t2/cpu.stat_percpu_all
