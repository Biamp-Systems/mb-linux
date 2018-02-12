/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_MMU_CONTEXT_H
#define _ASM_MICROBLAZE_MMU_CONTEXT_H

#ifndef CONFIG_MPU
# define init_new_context(tsk, mm)		({ 0; })

# define enter_lazy_tlb(mm, tsk)		do {} while (0)
# define change_mm_context(old, ctx, _pml4)	do {} while (0)
# define destroy_context(mm)			do {} while (0)
# define deactivate_mm(tsk, mm)			do {} while (0)
# define switch_mm(prev, next, tsk)		do {} while (0)
# define activate_mm(prev, next)		do {} while (0)

#else /* CONFIG_MPU */

# define enter_lazy_tlb(mm, tsk)		do {} while (0)
# define change_mm_context(old, ctx, _pml4)	do {} while (0)
# define destroy_context(mm)			do {} while (0)
# define deactivate_mm(tsk, mm)			do {} while (0)
# define activate_mm(prev, next)		do {} while (0)

static inline void tlb_write(int tlbindex, unsigned long tlbhi, unsigned long tlblo) {
  __asm__ __volatile__ (
    "mts rtlbx,  %2 \n"
    "mts rtlbhi, %0 \n"
    "mts rtlblom %1 \n"
    :: "r" (tlbhi),
       "r" (tlblo),
       "r" (tlbindex));
}

static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm) {
  return 0;
}

static inline void protect_page(struct mm_struct *mm, unsigned long addr,
            unsigned long flags) {
}

static inline void update_protections(struct mm_struct *mm) {
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk) {
}
#endif

#endif /* _ASM_MICROBLAZE_MMU_CONTEXT_H */
