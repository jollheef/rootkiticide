/**
 * @file hw_breakpoint.c
 * @author Mikhail Klementyev <jollheef@riseup.net>
 * @date May 2017
 * @brief abstraction layer for hardware breakpoint setup
 *
 * Currently implementation based on kernel AL, but we need (TODO)
 * own per each cpu architecture.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

__attribute__((warn_unused_result))
int is_kernel_address_valid(ulong addr)
{
	ulong above = ((long)addr) >> __VIRTUAL_MASK_SHIFT;
        if (above != 0 && above != -1UL)
                return 0;

        pgd_t *pgd = pgd_offset(current->mm, addr);
        if (pgd_none(*pgd))
                return 0;

        pud_t *pud = pud_offset(pgd, addr);
        if (pud_none(*pud))
                return 0;

        if (pud_large(*pud))
                return pfn_valid(pud_pfn(*pud));

        pmd_t *pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd))
                return 0;

        if (pmd_large(*pmd))
                return pfn_valid(pmd_pfn(*pmd));

        pte_t *pte = pte_offset_kernel(pmd, addr);
        if (pte_none(*pte))
                return 0;

        return pfn_valid(pte_pfn(*pte));
}

__attribute__((warn_unused_result)) struct perf_event * __percpu *
hbp_on_exec(const char *const funcname,
	    const perf_overflow_handler_t handler)
{
	struct perf_event_attr attr;
	hw_breakpoint_init(&attr);

	attr.bp_addr = kallsyms_lookup_name(funcname);
	if (!attr.bp_addr || !is_kernel_address_valid(attr.bp_addr))
		return ERR_PTR(-EINVAL);

	attr.bp_len = HW_BREAKPOINT_LEN_8;
	attr.bp_type = HW_BREAKPOINT_X;

	return register_wide_hw_breakpoint(&attr, handler, NULL);
}

void hbp_clear(struct perf_event * __percpu *hbp)
{
	unregister_wide_hw_breakpoint(hbp);
}
