// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Google LLC
 * Author: Mostafa Saleh <smostafa@google.com>
 */

#include <nvhe/pkvm.h>
#include <nvhe/spinlock.h>

struct hvc_power_domain {
	struct kvm_power_domain			*pd;
	const struct kvm_power_domain_ops	*ops;
};

struct hvc_power_domain handlers[MAX_POWER_DOMAINS];
static DEFINE_HYP_SPINLOCK(hvc_power_domain_lock);

int pkvm_init_hvc_pd(struct kvm_power_domain *pd,
		     const struct kvm_power_domain_ops *ops)
{
	struct hvc_power_domain *handler;
	unsigned int id;
	int ret = 0;

	if (pd->device_id >= MAX_POWER_DOMAINS)
		return -E2BIG;

	id = array_index_nospec(pd->device_id, MAX_POWER_DOMAINS);
	handler = &handlers[id];
	hyp_spin_lock(&hvc_power_domain_lock);
	if (handler->ops) {
		if (handler->pd != pd || handler->ops != ops)
			ret = -EEXIST;
	} else {
		handler->pd = pd;
		smp_store_release(&handler->ops, ops);
	}
	hyp_spin_unlock(&hvc_power_domain_lock);
	return ret;
}

int pkvm_host_hvc_pd(u64 device_id, u64 on)
{
	struct hvc_power_domain *pd;
	const struct kvm_power_domain_ops *ops;

	if (device_id >= MAX_POWER_DOMAINS)
		return -E2BIG;

	device_id = array_index_nospec(device_id, MAX_POWER_DOMAINS);
	pd = &handlers[device_id];

	ops = smp_load_acquire(&pd->ops);
	if (!ops)
		return -ENOENT;

	if (on)
		return ops->power_on(pd->pd);

	return ops->power_off(pd->pd);
}
