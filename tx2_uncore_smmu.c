// SPDX-License-Identifier: GPL-2.0
/*
 * CAVIUM THUNDERX2 SoC PMU UNCORE SMMU
 * Copyright (C) 2018 Cavium Inc.
 * Author: Ganapatrao Kulkarni <gkulkarni@cavium.com>
 */

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/cpuhotplug.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>

#define TX2_PMU_HRTIMER_INTERVAL	(1 * NSEC_PER_SEC)
#define GET_EVENTID(ev)			((ev->hw.config) & 0xff)
#define GET_COUNTERID(ev)		((ev->hw.idx) & 0xff)

#define TX2_PMU_SMMU_MAX_COUNTERS	32

/* Register offset */
#define SMMU_INTERRUPT                   0x412
#define SMMU_INTERRUPT_EN                0x413
#define SMMU_PGSZ_PREFERENCE             0x414
#define SMMU_RAM_PARITY_ERROR_LOG        0x415
#define SMMU_BSI_ERROR_LOG               0x416
#define SMMU_BSI_ERROR_H_LOG             0x417
#define SMMU_BMI_ERROR_LOG               0x418
#define SMMU_BMI_ERROR_H_LOG             0x419
#define SMMU_TIMEOUT_INV                 0x41a
#define SMMU_TIMEOUT_INTERNAL_INV        0x41b
#define SMMU_TIMEOUT_BSI                 0x41c
#define SMMU_TIMEOUT_PAGEWALKER          0x41d
#define SMMU_OVERRIDE_IDR                0x41e
#define SMMU_PERF_CTL                    0x420
#define SMMU_PERF_FILTER_ARID            0x421
#define SMMU_PERF_NUM_CYCLES             0x422
#define SMMU_PERF_NUM_CYCLES_H           0x423
#define SMMU_PERF_ARID_CONT_CACHE_HIT    0x424
#define SMMU_PERF_ARID_CONT_CACHE_HIT_H  0x425
#define SMMU_PERF_ARID_CONT_CACHE_MISS   0x426
#define SMMU_PERF_ARID_CONT_CACHE_MISS_H 0x427
#define SMMU_PERF_ARID_CONT_CACHE_EVICT  0x428
#define SMMU_PERF_ARID_CACHE_HIT         0x42a
#define SMMU_PERF_ARID_CACHE_HIT_H       0x42b
#define SMMU_PERF_ARID_CACHE_MISS        0x42c
#define SMMU_PERF_ARID_CACHE_MISS_H      0x42d
#define SMMU_PERF_ARID_CACHE_EVICT       0x42e
#define SMMU_PERF_MAIN_TLB_HIT           0x430
#define SMMU_PERF_MAIN_TLB_HIT_H         0x431
#define SMMU_PERF_MAIN_TLB_MISS          0x432
#define SMMU_PERF_MAIN_TLB_MISS_H        0x433
#define SMMU_PERF_MAIN_TLB_EVICT         0x434
#define SMMU_PERF_PWC_HIT                0x436
#define SMMU_PERF_PWC_HIT_H              0x437
#define SMMU_PERF_PWC_MISS               0x438
#define SMMU_PERF_PWC_MISS_H             0x439
#define SMMU_PERF_PWC_EVICT              0x43a
#define SMMU_PERF_PRIQ_REQ               0x43b
#define SMMU_PERF_ARID_INVALIDATION      0x43c
#define SMMU_PERF_TLB_INVALIDATION       0x43d
#define SMMU_PERF_DEVICE_INVALIDATION    0x43e
#define SMMU_PERF_WALKERS_FULL           0x43f
#define SMMU_PERF_TLB_PGSZ_4K_HIT        0x440
#define SMMU_PERF_TLB_PGSZ_64K_HIT       0x441
#define SMMU_PERF_TLB_PGSZ_2M_HIT        0x442
#define SMMU_PERF_TLB_PGSZ_32M_HIT       0x443
#define SMMU_PERF_TLB_PGSZ_512M_HIT      0x444
#define SMMU_PERF_TLB_PGSZ_1G_HIT        0x445
#define SMMU_PERF_TLB_PGSZ_16G_HIT       0x446

#define SMMU_BASE_ADDR	0x402300000
#define SMMU_BASE(node, smmu)	\
	(SMMU_BASE_ADDR + (node * 0x40000000) + (smmu * 0x20000))

#define dump_register(nm)	\
{				\
	.name	= #nm,		\
	.offset	= nm * 4,		\
}

#define SMMU_SMMU_CNTL                        0x410
#define SMMU_SMMU_CNTL_1                      0x411
#define SMMU_SMMU_INTERRUPT                   0x412
#define SMMU_SMMU_INTERRUPT_EN                0x413
#define SMMU_SMMU_PGSZ_PREFERENCE             0x414
#define SMMU_SMMU_RAM_PARITY_ERROR_LOG        0x415
#define SMMU_SMMU_BSI_ERROR_LOG               0x416
#define SMMU_SMMU_BSI_ERROR_H_LOG             0x417
#define SMMU_SMMU_BMI_ERROR_LOG               0x418
#define SMMU_SMMU_BMI_ERROR_H_LOG             0x419
#define SMMU_SMMU_TIMEOUT_INV                 0x41a
#define SMMU_SMMU_TIMEOUT_INTERNAL_INV        0x41b
#define SMMU_SMMU_TIMEOUT_BSI                 0x41c
#define SMMU_SMMU_TIMEOUT_PAGEWALKER          0x41d
#define SMMU_SMMU_OVERRIDE_IDR                0x41e
#define SMMU_SMMU_PERF_CTL                    0x420
#define SMMU_SMMU_PERF_FILTER_ARID            0x421
#define SMMU_SMMU_PERF_NUM_CYCLES             0x422
#define SMMU_SMMU_PERF_NUM_CYCLES_H           0x423
#define SMMU_SMMU_PERF_ARID_CONT_CACHE_HIT    0x424
#define SMMU_SMMU_PERF_ARID_CONT_CACHE_HIT_H  0x425
#define SMMU_SMMU_PERF_ARID_CONT_CACHE_MISS   0x426
#define SMMU_SMMU_PERF_ARID_CONT_CACHE_MISS_H 0x427
#define SMMU_SMMU_PERF_ARID_CONT_CACHE_EVICT  0x428
#define SMMU_SMMU_PERF_ARID_CACHE_HIT         0x42a
#define SMMU_SMMU_PERF_ARID_CACHE_HIT_H       0x42b
#define SMMU_SMMU_PERF_ARID_CACHE_MISS        0x42c
#define SMMU_SMMU_PERF_ARID_CACHE_MISS_H      0x42d
#define SMMU_SMMU_PERF_ARID_CACHE_EVICT       0x42e
#define SMMU_SMMU_PERF_MAIN_TLB_HIT           0x430
#define SMMU_SMMU_PERF_MAIN_TLB_HIT_H         0x431
#define SMMU_SMMU_PERF_MAIN_TLB_MISS          0x432
#define SMMU_SMMU_PERF_MAIN_TLB_MISS_H        0x433
#define SMMU_SMMU_PERF_MAIN_TLB_EVICT         0x434
#define SMMU_SMMU_PERF_PWC_HIT                0x436
#define SMMU_SMMU_PERF_PWC_HIT_H              0x437
#define SMMU_SMMU_PERF_PWC_MISS               0x438
#define SMMU_SMMU_PERF_PWC_MISS_H             0x439
#define SMMU_SMMU_PERF_PWC_EVICT              0x43a
#define SMMU_SMMU_PERF_PRIQ_REQ               0x43b
#define SMMU_SMMU_PERF_ARID_INVALIDATION      0x43c
#define SMMU_SMMU_PERF_TLB_INVALIDATION       0x43d
#define SMMU_SMMU_PERF_DEVICE_INVALIDATION    0x43e
#define SMMU_SMMU_PERF_WALKERS_FULL           0x43f
#define SMMU_SMMU_PERF_TLB_PGSZ_4K_HIT        0x440
#define SMMU_SMMU_PERF_TLB_PGSZ_64K_HIT       0x441
#define SMMU_SMMU_PERF_TLB_PGSZ_2M_HIT        0x442
#define SMMU_SMMU_PERF_TLB_PGSZ_32M_HIT       0x443
#define SMMU_SMMU_PERF_TLB_PGSZ_512M_HIT      0x444
#define SMMU_SMMU_PERF_TLB_PGSZ_1G_HIT        0x445
#define SMMU_SMMU_PERF_TLB_PGSZ_16G_HIT       0x446

static const struct debugfs_reg32 asmmu_debug_regs[] = {
	dump_register(SMMU_SMMU_CNTL),
	dump_register(SMMU_SMMU_CNTL_1),            
	dump_register(SMMU_SMMU_INTERRUPT),
	dump_register(SMMU_SMMU_INTERRUPT_EN),
	dump_register(SMMU_SMMU_PGSZ_PREFERENCE),
	dump_register(SMMU_SMMU_RAM_PARITY_ERROR_LOG),
	dump_register(SMMU_SMMU_BSI_ERROR_LOG),
	dump_register(SMMU_SMMU_BSI_ERROR_H_LOG),   
	dump_register(SMMU_SMMU_BMI_ERROR_LOG), 
	dump_register(SMMU_SMMU_BMI_ERROR_H_LOG),   
	dump_register(SMMU_SMMU_TIMEOUT_INV), 
	dump_register(SMMU_SMMU_TIMEOUT_INTERNAL_INV),
	dump_register(SMMU_SMMU_TIMEOUT_BSI),
	dump_register(SMMU_SMMU_TIMEOUT_PAGEWALKER),
	dump_register(SMMU_SMMU_OVERRIDE_IDR),
	dump_register(SMMU_SMMU_PERF_CTL),
	dump_register(SMMU_SMMU_PERF_FILTER_ARID),
	dump_register(SMMU_SMMU_PERF_NUM_CYCLES),
	dump_register(SMMU_SMMU_PERF_NUM_CYCLES_H),
	dump_register(SMMU_SMMU_PERF_ARID_CONT_CACHE_HIT),
	dump_register(SMMU_SMMU_PERF_ARID_CONT_CACHE_HIT_H),
	dump_register(SMMU_SMMU_PERF_ARID_CONT_CACHE_MISS),  
	dump_register(SMMU_SMMU_PERF_ARID_CONT_CACHE_MISS_H),
	dump_register(SMMU_SMMU_PERF_ARID_CONT_CACHE_EVICT),
	dump_register(SMMU_SMMU_PERF_ARID_CACHE_HIT), 
	dump_register(SMMU_SMMU_PERF_ARID_CACHE_HIT_H),
	dump_register(SMMU_SMMU_PERF_ARID_CACHE_MISS),       
	dump_register(SMMU_SMMU_PERF_ARID_CACHE_MISS_H),
	dump_register(SMMU_SMMU_PERF_ARID_CACHE_EVICT),    
	dump_register(SMMU_SMMU_PERF_MAIN_TLB_HIT),          
	dump_register(SMMU_SMMU_PERF_MAIN_TLB_HIT_H),
	dump_register(SMMU_SMMU_PERF_MAIN_TLB_MISS),       
	dump_register(SMMU_SMMU_PERF_MAIN_TLB_MISS_H),
	dump_register(SMMU_SMMU_PERF_MAIN_TLB_EVICT),      
	dump_register(SMMU_SMMU_PERF_PWC_HIT),       
	dump_register(SMMU_SMMU_PERF_PWC_HIT_H),
	dump_register(SMMU_SMMU_PERF_PWC_MISS),            
	dump_register(SMMU_SMMU_PERF_PWC_MISS_H),
	dump_register(SMMU_SMMU_PERF_PWC_EVICT),           
	dump_register(SMMU_SMMU_PERF_PRIQ_REQ),            
	dump_register(SMMU_SMMU_PERF_ARID_INVALIDATION),
	dump_register(SMMU_SMMU_PERF_TLB_INVALIDATION),    
	dump_register(SMMU_SMMU_PERF_DEVICE_INVALIDATION),
	dump_register(SMMU_SMMU_PERF_WALKERS_FULL),  
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_4K_HIT),
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_64K_HIT),      
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_2M_HIT),     
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_32M_HIT),      
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_512M_HIT),     
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_1G_HIT),    
	dump_register(SMMU_SMMU_PERF_TLB_PGSZ_16G_HIT),
};


static const unsigned long smmu_event_hw_offset[] = {
	SMMU_PERF_NUM_CYCLES,
	SMMU_PERF_ARID_CONT_CACHE_HIT,
	SMMU_PERF_ARID_CONT_CACHE_MISS,
	SMMU_PERF_ARID_CONT_CACHE_EVICT,
	SMMU_PERF_ARID_CACHE_HIT,
	SMMU_PERF_ARID_CACHE_MISS,
	SMMU_PERF_ARID_CACHE_EVICT,
	SMMU_PERF_MAIN_TLB_HIT,
	SMMU_PERF_MAIN_TLB_MISS,
	SMMU_PERF_MAIN_TLB_EVICT,
	SMMU_PERF_PWC_HIT,
	SMMU_PERF_PWC_MISS,
	SMMU_PERF_PWC_EVICT,
	SMMU_PERF_PRIQ_REQ,
	SMMU_PERF_ARID_INVALIDATION,
	SMMU_PERF_TLB_INVALIDATION,
	SMMU_PERF_DEVICE_INVALIDATION,
	SMMU_PERF_TLB_PGSZ_4K_HIT,
	SMMU_PERF_TLB_PGSZ_64K_HIT,
	SMMU_PERF_TLB_PGSZ_2M_HIT,
	SMMU_PERF_TLB_PGSZ_32M_HIT,
	SMMU_PERF_TLB_PGSZ_512M_HIT,
	SMMU_PERF_TLB_PGSZ_1G_HIT,
	SMMU_PERF_TLB_PGSZ_16G_HIT,
};

enum SMMU_PERF_EVENTS {
	SMMU_PERF_EVENT_NUM_CYCLES,
	SMMU_PERF_EVENT_ARID_CONT_CACHE_HIT,
	SMMU_PERF_EVENT_ARID_CONT_CACHE_MISS,
	SMMU_PERF_EVENT_ARID_CONT_CACHE_EVICT,
	SMMU_PERF_EVENT_ARID_CACHE_HIT,
	SMMU_PERF_EVENT_ARID_CACHE_MISS,
	SMMU_PERF_EVENT_ARID_CACHE_EVICT,
	SMMU_PERF_EVENT_MAIN_TLB_HIT,
	SMMU_PERF_EVENT_MAIN_TLB_MISS,
	SMMU_PERF_EVENT_MAIN_TLB_EVICT,
	SMMU_PERF_EVENT_PWC_HIT,
	SMMU_PERF_EVENT_PWC_MISS,
	SMMU_PERF_EVENT_PWC_EVICT,
	SMMU_PERF_EVENT_PRIQ_REQ,
	SMMU_PERF_EVENT_ARID_INVALIDATION,
	SMMU_PERF_EVENT_TLB_INVALIDATION,
	SMMU_PERF_EVENT_DEVICE_INVALIDATION,
	SMMU_PERF_EVENT_TLB_PGSZ_4K_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_64K_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_2M_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_32M_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_512M_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_1G_HIT,
	SMMU_PERF_EVENT_TLB_PGSZ_16G_HIT,
	SMMU_PERF_EVENT_MAX
};

struct tx2_uncore_pmu {
	struct hlist_node hpnode;
	struct list_head  entry;
	struct pmu pmu;
	char *name;
	int node;
	int cpu;
	u32 max_counters;
	u32 max_events;
	u64 hrtimer_interval;
	void __iomem *base;
	DECLARE_BITMAP(active_counters, TX2_PMU_SMMU_MAX_COUNTERS);
	struct perf_event *events[TX2_PMU_SMMU_MAX_COUNTERS];
	struct hrtimer hrtimer;
	const struct attribute_group **attr_groups;
};

static LIST_HEAD(tx2_pmus);

static inline struct tx2_uncore_pmu *pmu_to_tx2_pmu(struct pmu *pmu)
{
	return container_of(pmu, struct tx2_uncore_pmu, pmu);
}

PMU_FORMAT_ATTR(event,	"config:0-9");

static struct attribute *smmu_pmu_format_attrs[] = {
	&format_attr_event.attr,
	NULL,
};

static const struct attribute_group smmu_pmu_format_attr_group = {
	.name = "format",
	.attrs = smmu_pmu_format_attrs,
};

/*
 * sysfs event attributes
 */
static ssize_t tx2_pmu_event_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct dev_ext_attribute *eattr;

	eattr = container_of(attr, struct dev_ext_attribute, attr);
	return sprintf(buf, "event=0x%lx\n", (unsigned long) eattr->var);
}

#define TX2_EVENT_ATTR(name, config) \
	PMU_EVENT_ATTR(name, tx2_pmu_event_attr_##name, \
			config, tx2_pmu_event_show)

TX2_EVENT_ATTR(cycles, SMMU_PERF_EVENT_NUM_CYCLES);
TX2_EVENT_ATTR(aridcont_cache_miss, SMMU_PERF_EVENT_ARID_CONT_CACHE_MISS);
TX2_EVENT_ATTR(arid_cache_miss, SMMU_PERF_EVENT_ARID_CACHE_MISS);
TX2_EVENT_ATTR(tlb_hit, SMMU_PERF_EVENT_MAIN_TLB_HIT);
TX2_EVENT_ATTR(tlb_miss, SMMU_PERF_EVENT_MAIN_TLB_MISS);
TX2_EVENT_ATTR(tlb_evict, SMMU_PERF_EVENT_MAIN_TLB_EVICT);
TX2_EVENT_ATTR(pwc_hit, SMMU_PERF_EVENT_PWC_HIT);
TX2_EVENT_ATTR(pwc_miss, SMMU_PERF_EVENT_PWC_MISS);
TX2_EVENT_ATTR(pwc_evict, SMMU_PERF_EVENT_PWC_EVICT);
TX2_EVENT_ATTR(priq_req, SMMU_PERF_EVENT_PRIQ_REQ);
TX2_EVENT_ATTR(tlb_inv, SMMU_PERF_EVENT_TLB_INVALIDATION);
TX2_EVENT_ATTR(tlb_hit_4k, SMMU_PERF_EVENT_TLB_PGSZ_4K_HIT);
TX2_EVENT_ATTR(tlb_hit_64k, SMMU_PERF_EVENT_TLB_PGSZ_64K_HIT);
TX2_EVENT_ATTR(tlb_hit_2m, SMMU_PERF_EVENT_TLB_PGSZ_2M_HIT);
TX2_EVENT_ATTR(tlb_hit_512m, SMMU_PERF_EVENT_TLB_PGSZ_512M_HIT);

static struct attribute *smmu_pmu_events_attrs[] = {
	&tx2_pmu_event_attr_cycles.attr.attr,
	&tx2_pmu_event_attr_aridcont_cache_miss.attr.attr,
	&tx2_pmu_event_attr_arid_cache_miss.attr.attr,
	&tx2_pmu_event_attr_tlb_hit.attr.attr,
	&tx2_pmu_event_attr_tlb_miss.attr.attr,
	&tx2_pmu_event_attr_tlb_evict.attr.attr,
	&tx2_pmu_event_attr_pwc_hit.attr.attr,
	&tx2_pmu_event_attr_pwc_miss.attr.attr,
	&tx2_pmu_event_attr_pwc_evict.attr.attr,
	&tx2_pmu_event_attr_priq_req.attr.attr,
	&tx2_pmu_event_attr_tlb_inv.attr.attr,
	&tx2_pmu_event_attr_tlb_hit_4k.attr.attr,
	&tx2_pmu_event_attr_tlb_hit_64k.attr.attr,
	&tx2_pmu_event_attr_tlb_hit_2m.attr.attr,
	&tx2_pmu_event_attr_tlb_hit_512m.attr.attr,
	NULL
};

static const struct attribute_group smmu_pmu_events_attr_group = {
	.name = "events",
	.attrs = smmu_pmu_events_attrs,
};

/*
 * sysfs cpumask attributes
 */
static ssize_t cpumask_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct tx2_uncore_pmu *tx2_pmu;

	tx2_pmu = pmu_to_tx2_pmu(dev_get_drvdata(dev));
	return cpumap_print_to_pagebuf(true, buf, cpumask_of(tx2_pmu->cpu));
}
static DEVICE_ATTR_RO(cpumask);

static struct attribute *tx2_pmu_cpumask_attrs[] = {
	&dev_attr_cpumask.attr,
	NULL,
};

static const struct attribute_group pmu_cpumask_attr_group = {
	.attrs = tx2_pmu_cpumask_attrs,
};

/*
 * Per PMU device attribute groups
 */
static const struct attribute_group *smmu_pmu_attr_groups[] = {
	&smmu_pmu_format_attr_group,
	&pmu_cpumask_attr_group,
	&smmu_pmu_events_attr_group,
	NULL
};

static inline u32 reg_readl(unsigned long addr)
{
	return readl((void __iomem *)addr);
}

static inline void reg_writel(u32 val, unsigned long addr)
{
	writel(val, (void __iomem *)addr);
}

static int alloc_counter(struct tx2_uncore_pmu *tx2_pmu)
{
	int counter;

	counter = find_first_zero_bit(tx2_pmu->active_counters,
				tx2_pmu->max_counters);
	if (counter == tx2_pmu->max_counters)
		return -ENOSPC;

	set_bit(counter, tx2_pmu->active_counters);
	return counter;
}

static inline void free_counter(struct tx2_uncore_pmu *tx2_pmu, int counter)
{
	clear_bit(counter, tx2_pmu->active_counters);
}

static bool is_event_64bit(struct perf_event *event)
{

	switch (GET_EVENTID(event)) {
	case SMMU_PERF_EVENT_NUM_CYCLES :
	case SMMU_PERF_EVENT_ARID_CONT_CACHE_HIT :
	case SMMU_PERF_EVENT_ARID_CONT_CACHE_MISS :
	case SMMU_PERF_EVENT_ARID_CACHE_HIT :
	case SMMU_PERF_EVENT_ARID_CACHE_MISS :
	case SMMU_PERF_EVENT_MAIN_TLB_HIT :
	case SMMU_PERF_EVENT_MAIN_TLB_MISS :
	case SMMU_PERF_EVENT_PWC_HIT :
	case SMMU_PERF_EVENT_PWC_MISS :
		return 1;
	default:
		return 0;
	}
}

static void tx2_uncore_event_update(struct perf_event *event)
{
	s64 new = 0;
	struct hw_perf_event *hwc = &event->hw;
	struct tx2_uncore_pmu *tx2_pmu;

	tx2_pmu = pmu_to_tx2_pmu(event->pmu);

	new = reg_readl(hwc->event_base);
	if (is_event_64bit(event))
		new |=  (u64)reg_readl(hwc->event_base + 4) << 32;
	local64_add(new , &event->count);
}

static bool tx2_uncore_validate_event(struct pmu *pmu,
				  struct perf_event *event, int *counters)
{
	if (is_software_event(event))
		return true;
	/* Reject groups spanning multiple HW PMUs. */
	if (event->pmu != pmu)
		return false;

	*counters = *counters + 1;
	return true;
}

/*
 * Make sure the group of events can be scheduled at once
 * on the PMU.
 */
static bool tx2_uncore_validate_event_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	int counters = 0;
	struct tx2_uncore_pmu *tx2_pmu;

	tx2_pmu = pmu_to_tx2_pmu(event->pmu);

	if (event->group_leader == event)
		return true;

	if (!tx2_uncore_validate_event(event->pmu, leader, &counters))
		return false;

	for_each_sibling_event(sibling, leader) {
		if (!tx2_uncore_validate_event(event->pmu, sibling, &counters))
			return false;
	}

	if (!tx2_uncore_validate_event(event->pmu, event, &counters))
		return false;

	/*
	 * If the group requires more counters than the HW has,
	 * it cannot ever be scheduled.
	 */
	return counters < tx2_pmu->max_counters;
}

static int tx2_uncore_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct tx2_uncore_pmu *tx2_pmu;

	/*
	 * SOC PMU counters are shared across all cores.
	 * Therefore, it does not support per-process mode.
	 * Also, it does not support event sampling mode.
	 */
	if (is_sampling_event(event) || event->attach_state & PERF_ATTACH_TASK)
		return -EINVAL;

	/* We have no filtering of any kind */
	if (event->attr.exclude_user	||
	    event->attr.exclude_kernel	||
	    event->attr.exclude_hv	||
	    event->attr.exclude_idle	||
	    event->attr.exclude_host	||
	    event->attr.exclude_guest)
		return -EINVAL;

	if (event->cpu < 0)
		return -EINVAL;

	tx2_pmu = pmu_to_tx2_pmu(event->pmu);
	if (tx2_pmu->cpu >= nr_cpu_ids)
		return -EINVAL;
	event->cpu = tx2_pmu->cpu;

	if (event->attr.config >= tx2_pmu->max_events)
		return -EINVAL;

	/* store event id */
	hwc->config = event->attr.config;

	/* Validate the group */
	if (!tx2_uncore_validate_event_group(event))
		return -EINVAL;

	return 0;
}

static void tx2_uncore_event_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct tx2_uncore_pmu *tx2_pmu;

	hwc->state = 0;
	tx2_pmu = pmu_to_tx2_pmu(event->pmu);

	reg_writel(0xfffffc07, hwc->config_base);
	local64_set(&event->hw.prev_count, 0ULL);

	perf_event_update_userpage(event);

	/* Start timer for first event */
	if (bitmap_weight(tx2_pmu->active_counters,
				tx2_pmu->max_counters) == 1) {
		hrtimer_start(&tx2_pmu->hrtimer,
			ns_to_ktime(tx2_pmu->hrtimer_interval),
			HRTIMER_MODE_REL_PINNED);
	}
}

static void tx2_uncore_event_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct tx2_uncore_pmu *tx2_pmu;

	if (hwc->state & PERF_HES_UPTODATE)
		return;

	tx2_pmu = pmu_to_tx2_pmu(event->pmu);

	/* select, disable and stop counter */
	reg_writel(0, hwc->config_base);

	WARN_ON_ONCE(hwc->state & PERF_HES_STOPPED);
	hwc->state |= PERF_HES_STOPPED;
	if (flags & PERF_EF_UPDATE) {
		tx2_uncore_event_update(event);
		hwc->state |= PERF_HES_UPTODATE;
	}
}

static int tx2_uncore_event_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct tx2_uncore_pmu *tx2_pmu;

	tx2_pmu = pmu_to_tx2_pmu(event->pmu);

	/* Allocate a free counter */
	hwc->idx  = alloc_counter(tx2_pmu);
	if (hwc->idx < 0)
		return -EAGAIN;

	tx2_pmu->events[hwc->idx] = event;
	/* set counter control and data registers base address */
	hwc->config_base = (unsigned long)tx2_pmu->base +
		SMMU_PERF_CTL * 4;
	hwc->event_base =  (unsigned long)tx2_pmu->base +
		smmu_event_hw_offset[GET_EVENTID(event)] * 4;

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (flags & PERF_EF_START)
		tx2_uncore_event_start(event, flags);

	return 0;
}

static void tx2_uncore_event_del(struct perf_event *event, int flags)
{
	struct tx2_uncore_pmu *tx2_pmu = pmu_to_tx2_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	tx2_uncore_event_stop(event, PERF_EF_UPDATE);

	/* clear the assigned counter */
	free_counter(tx2_pmu, GET_COUNTERID(event));

	perf_event_update_userpage(event);
	tx2_pmu->events[hwc->idx] = NULL;
	hwc->idx = -1;
}

static void tx2_uncore_event_read(struct perf_event *event)
{
	tx2_uncore_event_update(event);
}

static enum hrtimer_restart tx2_hrtimer_callback(struct hrtimer *timer)
{
	struct tx2_uncore_pmu *tx2_pmu;
	int max_counters, idx;
	struct perf_event *event = NULL;

	tx2_pmu = container_of(timer, struct tx2_uncore_pmu, hrtimer);
	max_counters = tx2_pmu->max_counters;

	if (bitmap_empty(tx2_pmu->active_counters, max_counters))
		return HRTIMER_NORESTART;

	for_each_set_bit(idx, tx2_pmu->active_counters, max_counters) {
		event = tx2_pmu->events[idx];
		tx2_uncore_event_update(event);
	}

	for_each_set_bit(idx, tx2_pmu->active_counters, max_counters) {
		event = tx2_pmu->events[idx];
		/* Start counter again */
		reg_writel(0xfffffc07, event->hw.config_base);
		break;
	}

	hrtimer_forward_now(timer, ns_to_ktime(tx2_pmu->hrtimer_interval));
	return HRTIMER_RESTART;
}

static int tx2_uncore_pmu_register(
		struct tx2_uncore_pmu *tx2_pmu)
{
	char *name = tx2_pmu->name;

	/* Perf event registration */
	tx2_pmu->pmu = (struct pmu) {
		.module         = THIS_MODULE,
		.attr_groups	= tx2_pmu->attr_groups,
		.task_ctx_nr	= perf_invalid_context,
		.event_init	= tx2_uncore_event_init,
		.add		= tx2_uncore_event_add,
		.del		= tx2_uncore_event_del,
		.start		= tx2_uncore_event_start,
		.stop		= tx2_uncore_event_stop,
		.read		= tx2_uncore_event_read,
	};

	tx2_pmu->pmu.name = kasprintf(GFP_KERNEL, "%s", name);

	return perf_pmu_register(&tx2_pmu->pmu, tx2_pmu->pmu.name, -1);
}

static int tx2_uncore_pmu_add_dev(struct tx2_uncore_pmu *tx2_pmu)
{
	int ret, cpu;

	cpu = cpumask_any_and(cpumask_of_node(tx2_pmu->node),
			cpu_online_mask);
	tx2_pmu->cpu = cpu;
	hrtimer_init(&tx2_pmu->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tx2_pmu->hrtimer.function = tx2_hrtimer_callback;

	ret = tx2_uncore_pmu_register(tx2_pmu);
	if (ret) {
		pr_err("%s PMU: Failed to init driver\n", tx2_pmu->name);
		return -ENODEV;
	}

	if (ret) {
		pr_err("Error %d registering hotplug", ret);
		return ret;
	}

	/* Add to list */
	list_add(&tx2_pmu->entry, &tx2_pmus);

	printk("%s SMMU PMU registered\n", tx2_pmu->pmu.name);
	return ret;
}

static struct tx2_uncore_pmu *tx2_uncore_pmu_init_dev(int node , int smmu)
{
	struct tx2_uncore_pmu *tx2_pmu;
	void __iomem *base;

	tx2_pmu = kzalloc(sizeof(*tx2_pmu), GFP_KERNEL);
	if (!tx2_pmu)
		return NULL;

	base = ioremap(SMMU_BASE(node, smmu), 0xffff);

	INIT_LIST_HEAD(&tx2_pmu->entry);
	tx2_pmu->base = base;
	tx2_pmu->node = node;
	tx2_pmu->max_counters = TX2_PMU_SMMU_MAX_COUNTERS;
	tx2_pmu->max_events = SMMU_PERF_EVENT_MAX;
	tx2_pmu->hrtimer_interval = TX2_PMU_HRTIMER_INTERVAL;
	tx2_pmu->attr_groups = smmu_pmu_attr_groups;
	tx2_pmu->name = kasprintf( GFP_KERNEL, "uncore_smmu_%d", (node * 3) + smmu);

	return tx2_pmu;
}


u32 asmmu_regval, asmmu_regno;
struct debugfs_regset32 *asmmu_get_regset(void __iomem *base)
{
	struct debugfs_regset32 *rset;

	rset = kmalloc(sizeof(struct debugfs_regset32), GFP_KERNEL);
	if (rset == NULL) {
		pr_err("kzallc failure\n");
		return NULL;
	}
	
	rset->regs = asmmu_debug_regs;
	rset->nregs = sizeof(asmmu_debug_regs)/sizeof(struct debugfs_reg32 );
	rset->base = base;

	return rset;
}

static int asmmu_regwrite_set(void *data, u64 val)
{
	void __iomem *base = data;

	if (val) {
		writel(asmmu_regval, base + asmmu_regno * 4);
		pr_err("0x%llx = 0x%x\n", (u64) base + asmmu_regno * 4, (u32)asmmu_regval);
	} 

	return 0;
}

static int asmmu_regwrite_get(void *data, u64 *val)
{

	return 0;
}

struct dentry *asmmu_debugfs_dir[8];
DEFINE_SIMPLE_ATTRIBUTE(asmmu_regwrite_fops, asmmu_regwrite_get,
			asmmu_regwrite_set, "%llu\n");

static int tx2_uncore_pmu_add(int node, int smmu)
{
	struct tx2_uncore_pmu *tx2_pmu;
	int smmu_id = node * 3 + smmu;

	tx2_pmu = tx2_uncore_pmu_init_dev(node, smmu);

	if (!tx2_pmu)
		return -1;

	if (tx2_uncore_pmu_add_dev(tx2_pmu)) {
		return -1;
	}


	asmmu_debugfs_dir[smmu_id] = debugfs_create_dir(tx2_pmu->name, NULL);
	if (asmmu_debugfs_dir == NULL)
		return -ENOMEM;

	debugfs_create_regset32("regdump", S_IRUGO, asmmu_debugfs_dir[smmu_id],
				asmmu_get_regset(tx2_pmu->base));

	debugfs_create_x32("regno", 0644, asmmu_debugfs_dir[smmu_id],
			   &asmmu_regno);
	debugfs_create_x32("value", 0644, asmmu_debugfs_dir[smmu_id],
			   &asmmu_regval);

	debugfs_create_file("write_cmd", 0644, asmmu_debugfs_dir[smmu_id],
			    tx2_pmu->base, &asmmu_regwrite_fops);

	return 0;
}

static int __init smmu_perf_init(void)
{
	int node, smmu;

	for_each_online_node(node) {
		for (smmu = 0; smmu < 3; smmu++)
			tx2_uncore_pmu_add(node, smmu);
	}

	pr_info("SMMU perf module loaded\n");
	return 0;
}

void smmu_perf_exit(void)
{
	struct tx2_uncore_pmu *tx2_pmu, *temp;

	if (!list_empty(&tx2_pmus)) {
		list_for_each_entry_safe(tx2_pmu, temp, &tx2_pmus, entry) {
			perf_pmu_unregister(&tx2_pmu->pmu);
			list_del(&tx2_pmu->entry);
			iounmap(tx2_pmu->base);
			kfree(tx2_pmu);
		}
	}
	pr_info("SMMU perf module unloaded\n");
}

module_init(smmu_perf_init);
module_exit(smmu_perf_exit);

MODULE_DESCRIPTION("ThunderX2 SMMU UNCORE PMU driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ganapatrao Kulkarni <gkulkarni@marvell.com>");
