// SPDX-License-Identifier: GPL-2.0
#include <linux/kallsyms.h>
#include <linux/cgroup.h>
#include <linux/version.h>

/*
 * On kernels 5.7+ kallsyms_lookup_name is not exported anymore,
 * so it is not usable in kernel modules. this is an alternative.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
#define KPROBE_LOOKUP 1
#include <linux/kprobes.h>
static struct kprobe kp = {
	.symbol_name = "kallsyms_lookup_name"
};
#endif

#define LOOKUP_SYMS(name)						\
do {									\
	m_##name = (void *)kallsyms_lookup_name(#name);			\
	if (!m_##name) {						\
		pr_err("kallsyms loopup failed: %s\n", #name);		\
		return -ENXIO;						\
	}								\
} while (0)

static int (*m_cgroup_add_dfl_cftypes)(struct cgroup_subsys *ss, struct cftype *cfts);
static int (*m_cgroup_rm_cftypes)(struct cftype *cfts);
static struct mutex *m_cgroup_mutex;
static struct cgroup_subsys *m_cpu_cgrp_subsys;

static inline bool cgroup_is_dead(const struct cgroup *cgrp)
{
	return !(cgrp->self.flags & CSS_ONLINE);
}

/* walk live descendants in pre order */
#define cgroup_for_each_live_descendant_pre(dsct, d_css, cgrp)		\
	css_for_each_descendant_pre((d_css), (&cgrp->self))		\
		if (({ lockdep_assert_held(m_cgroup_mutex);		\
		       (dsct) = (d_css)->cgroup;			\
		       cgroup_is_dead(dsct); }))			\
			;						\
		else

static inline void get_cgroup_percpu_stat(struct cgroup *cgrp, int cpu,
						struct cgroup_base_stat *stat)
{
	struct cgroup_rstat_cpu *rstatc;
	unsigned int seq;

	rstatc = per_cpu_ptr(cgrp->rstat_cpu, cpu);
	if (likely(rstatc)) {
		do {
			seq = __u64_stats_fetch_begin(&rstatc->bsync);
			*stat = rstatc->bstat;
		} while (__u64_stats_fetch_retry(&rstatc->bsync, seq));
	}
}

static int cstat_percpu_show_all(struct seq_file *m, void *v)
{
	struct cgroup_base_stat pstat, cstat;
	struct cgroup_subsys_state *d_css;
	struct cgroup_rstat_cpu *rstatc;
	struct cgroup *child, *cgrp;
	int cpu;

	cgrp = seq_css(m)->cgroup;
	for_each_possible_cpu(cpu) {
		memset(&pstat, 0, sizeof(struct cgroup_base_stat));
		mutex_lock(m_cgroup_mutex);
		cgroup_for_each_live_descendant_pre(child, d_css, cgrp) {
			get_cgroup_percpu_stat(child, cpu, &cstat);
			pstat.cputime.stime += cstat.cputime.stime;
			pstat.cputime.utime += cstat.cputime.utime;
			pstat.cputime.sum_exec_runtime += cstat.cputime.sum_exec_runtime;
		}
		mutex_unlock(m_cgroup_mutex);
		rstatc = per_cpu_ptr(cgrp->rstat_cpu, cpu);
		seq_printf(m, "%llu %llu %llu %llu %llu %llu\n", pstat.cputime.sum_exec_runtime, rstatc->cumul_bstat.cputime.sum_exec_runtime,
				pstat.cputime.stime, rstatc->cumul_bstat.cputime.stime, pstat.cputime.utime, rstatc->cumul_bstat.cputime.utime);
	}
	return 0;
}

/*
 * Before using this interface, you need to flush the data,
 * such as "cat cpu.stat".
 */
static struct cftype cstat_files[] = {
	{
		.name = "stat_percpu_all",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = cstat_percpu_show_all,
	},
	{ } /* terminate */
};

static int __init cstat_percpu_init(void)
{
#ifdef KPROBE_LOOKUP
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	kallsyms_lookup_name_t kallsyms_lookup_name;
	register_kprobe(&kp);
	kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
	unregister_kprobe(&kp);
#endif

	LOOKUP_SYMS(cgroup_add_dfl_cftypes);
	LOOKUP_SYMS(cgroup_rm_cftypes);
	LOOKUP_SYMS(cpu_cgrp_subsys);
	LOOKUP_SYMS(cgroup_mutex);

	return m_cgroup_add_dfl_cftypes(m_cpu_cgrp_subsys, cstat_files);
}

static void __exit cstat_percpu_exit(void)
{
	m_cgroup_rm_cftypes(cstat_files);
}

module_init(cstat_percpu_init);
module_exit(cstat_percpu_exit);
MODULE_LICENSE("GPL v2");
