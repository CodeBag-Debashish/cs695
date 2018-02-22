#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/unistd.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>

extern long (*address)(int *,char **);

asmlinkage void kill_sample_process(struct task_struct *task) {
	int signum = SIGKILL;
	struct siginfo info;
	memset(&info,0,sizeof(struct siginfo));
	info.si_signo = signum;
	int ret = send_sig_info(signum,&info,task);
	if(ret < 0) {
		printk(KERN_INFO "error sending signal\n");	
	}else {
		printk("sample process pid = %d is terminated\n",task->pid);
	}
}

asmlinkage long find_VSS(struct task_struct *task) {
	long value = 0;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	mm = task->mm;
	printk("This mm_struct has %d vmas.\n", mm->map_count);
	
	for(vma = mm->mmap; vma ;vma = vma->vm_next) {
		printk("Start = %p End = %p and perm = 0x%1x\n",vma->vm_start,vma->vm_end,vma->vm_page_prot.pgprot);

		value += (vma->vm_end - vma->vm_start)/**sizeof( *(vma->vm_start) )*/;
	}
	
	/*
	// we can use some of the following information 
	unsigned long total_vm;		// Total pages mapped
	unsigned long locked_vm;	// Pages that have PG_mlocked set 
	unsigned long pinned_vm;	// Refcount permanently increased 
	unsigned long data_vm;		// VM_WRITE & ~VM_SHARED & ~VM_STACK 
	unsigned long exec_vm;		// VM_EXEC & ~VM_WRITE & ~VM_STACK 
	unsigned long stack_vm;		// VM_STACK 
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;
	
	*/

	return value;

}


asmlinkage long page_table_walk(struct task_struct *task , unsigned long addr) {
	
	pgd_t *pgd;
	pte_t *ptep, pte,tmp_pte;
	pud_t *pud;
	pmd_t *pmd;
	
	unsigned long _pte_value = 0;
	
	struct page *page = NULL;
    	struct mm_struct *mm = task->mm;
	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) goto out;
	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud)) goto out;
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) goto out;
	ptep = pte_offset_map(pmd, addr);
	if (!ptep) goto out;
	pte = *ptep;
	
	//page = pte_page(pte);
	
	_pte_value = pte_val(pte);
	
	/*
	static inline pteval_t pte_val(pte_t pte)
	{
	pteval_t ret;
	if (sizeof(pteval_t) > sizeof(long))
		ret = PVOP_CALLEE2(pteval_t, pv_mmu_ops.pte_val,
				   pte.pte, (u64)pte.pte >> 32);
	else
		ret = PVOP_CALLEE1(pteval_t, pv_mmu_ops.pte_val,
				   pte.pte);
	return ret;
	}
	
	 *
	_PAGE_BIT_SOFTW1	9	available for programmer
	_PAGE_BIT_SOFTW2	10	same
	_PAGE_BIT_SOFTW3	11	same
	*/

	/*
		static inline pte_t pte_set_flags(pte_t pte, pteval_t set)
		{
			pteval_t v = native_pte_val(pte);
			return native_make_pte(v | set);
		}
	
	static inline void set_pte(pte_t *ptep, pte_t pte)
	{
		if (sizeof(pteval_t) > sizeof(long))
			PVOP_VCALL3(pv_mmu_ops.set_pte, ptep,
				    pte.pte, (u64)pte.pte >> 32);
		else
			PVOP_VCALL2(pv_mmu_ops.set_pte, ptep,
				    pte.pte);
	}
	*/


	if(_pte_value & 1) {
		
		
		if( !( _pte_value & ( 1 << _PAGE_BIT_SOFTW2 ) ) ) {
		
			

			printk(KERN_INFO "entry = 0x%1x",_pte_value);
			
			tmp_pte = pte;
			unsigned long m_mask = 1 << _PAGE_BIT_SOFTW2;
			set_pte( ptep , pte_set_flags(tmp_pte, m_mask) );
			

			unsigned long _new_pte_value = pte_val(*ptep);
			if(_pte_value != _new_pte_value) {
				printk(KERN_INFO "0x%1x <> 0x%1x",_pte_value,_new_pte_value);
				//printk(KERN_INFO "CHANGE\nD");
			}
			


			return 1;

		}else {

			printk(KERN_INFO "--------------------------LAREADY VISITED PAGE --------------------");
			return 0;
		}
		
		
		//return 1;
	}else {
		return 0;
	}
	
	//if(page != NULL) {
	//	//printk(KERN_INFO "page frame struct is @ %p", page);
	//	printk(KERN_INFO "entry = %ld",pte);
	//	return 1;
	//}

 out:
	return 0;
}	

asmlinkage long find_RSS(struct task_struct *task) {
	
	/*
	 *
	
	unsigned long vm_start;		Our start address within vm_mm. 
	unsigned long vm_end;		The first byte after our end address
					within vm_mm. 
	*
	*/

	int _PAGE_SIZE = 4096;
	long value = 0;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	mm = task->mm;
	unsigned long v_addr;
	
	//return value = get_mm_rss(mm);

	for(vma = mm->mmap; vma ; vma = vma->vm_next) {
		for(v_addr = vma->vm_start; v_addr < vma->vm_end;v_addr += _PAGE_SIZE) {
			if(page_table_walk(task,v_addr) == 1) {
				value += 1;
			}			
		}
	}

	return value;
}


asmlinkage long find_WSS(struct task_struct *task) {
	long value = 0;
	
	return value;
}

asmlinkage long print_all_parents(struct task_struct *task) {
	
	struct task_struct *prev;

	do {
		prev = task;
		
		if(task->parent != NULL) {
			printk(KERN_INFO ">> %s :: %d\n",task->parent->comm , task->parent->pid);
		}

		task = task->parent;
	}while(prev->pid != 0);

	return 0;
}

asmlinkage long hook_func(int *arg1,char **arg2) {
	printk(KERN_INFO "Kernel Hacked You can do all kinds of funny stuff here !\n");	
	struct task_struct *task;
	long vss,wss,rss;
	pid_t _pid = (pid_t)arg1[0];
	int found = 0;	
	for_each_process(task) {
		if(task->pid == _pid) {

			found = 1;
			break;
		}	
	}

	if(found) {
		
		msleep(10000);
		
		printk(KERN_INFO "process name = %s",task->comm);

		vss = find_VSS(task);
		rss = find_RSS(task);
		wss = find_WSS(task);
		
		printk(KERN_INFO "vss = %ld",vss);
		printk(KERN_INFO "rss = %ld",rss);
		printk(KERN_INFO "wss = %ld",wss);
		

		arg1[0] = _pid;
		arg1[1] = vss;
		arg1[2] = rss;
		arg1[3] = wss;
	//	arg2[0] = (char *)(task->comm);

		print_all_parents(task);

	}else {
		printk(KERN_INFO "process not found\n");
	}

	kill_sample_process(task);
	return 0;
}

static int init1_module_func(int _pid) {
	address = hook_func;
	printk(KERN_INFO "module loaded ! function placed at location =  %p \n",address);
	return 0;
}

static void exit1_module_func(void) {
	address = NULL;
	printk(KERN_INFO "module unloaded ! address var is set to NULL\n");
}

module_init(init1_module_func);
module_exit(exit1_module_func);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DEBASHISH DEKA");
MODULE_VERSION("0.1");
